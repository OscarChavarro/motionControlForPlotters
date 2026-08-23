#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static constexpr char DEVICE_NAME[] = "Vitral plotter servo control";
static constexpr char ADVERTISED_NAME[] = "Vitral plotter servo control";
static constexpr char IDLE_MESSAGE[] = "ESP-32 idle";
static constexpr char ECHO_PREFIX[] = "ESP-32 echo ";
static constexpr uint16_t INVALID_CONNECTION_HANDLE = 0xffff;
static constexpr size_t MAX_RECEIVED_MESSAGE_SIZE = 512;
static constexpr gpio_num_t STATUS_LED_GPIO = GPIO_NUM_2;
static constexpr TickType_t STATUS_LED_INTERVAL = pdMS_TO_TICKS(1000);
#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
static constexpr BaseType_t BLE_IDLE_TASK_CORE = tskNO_AFFINITY;
#else
static constexpr BaseType_t BLE_IDLE_TASK_CORE = 1;
#endif

// Nordic UART Service UUIDs. UUID bytes are stored least-significant first by
// NimBLE, as required by BLE_UUID128_INIT.
static const ble_uuid128_t SERIAL_SERVICE_UUID = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t SERIAL_RX_UUID = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static const ble_uuid128_t SERIAL_TX_UUID = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static uint8_t ownAddressType;
static uint16_t connectionHandle = INVALID_CONNECTION_HANDLE;
static uint16_t transmitValueHandle;
static bool notificationsEnabled;
static portMUX_TYPE connectionStateLock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t transmitMutex;

static void startAdvertising();

static void sendSerialBytes(const uint8_t *data, size_t size) {
    if (size == 0 || xSemaphoreTake(transmitMutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    portENTER_CRITICAL(&connectionStateLock);
    const uint16_t activeConnectionHandle = connectionHandle;
    const bool canNotify = notificationsEnabled;
    portEXIT_CRITICAL(&connectionStateLock);

    if (!canNotify || activeConnectionHandle == INVALID_CONNECTION_HANDLE) {
        xSemaphoreGive(transmitMutex);
        return;
    }

    const uint16_t mtu = ble_att_mtu(activeConnectionHandle);
    const size_t maximumChunkSize = mtu > 3 ? mtu - 3 : 20;

    for (size_t offset = 0; offset < size;) {
        const size_t remaining = size - offset;
        const size_t chunkSize =
            remaining < maximumChunkSize ? remaining : maximumChunkSize;
        os_mbuf *packet = ble_hs_mbuf_from_flat(data + offset, chunkSize);
        if (packet == nullptr) {
            xSemaphoreGive(transmitMutex);
            return;
        }

        const int result = ble_gatts_notify_custom(
            activeConnectionHandle, transmitValueHandle, packet);
        if (result != 0) {
            xSemaphoreGive(transmitMutex);
            return;
        }
        offset += chunkSize;
    }
    xSemaphoreGive(transmitMutex);
}

static int receiveSerialData(uint16_t,
                             uint16_t,
                             ble_gatt_access_ctxt *context,
                             void *) {
    const uint16_t receivedSize = OS_MBUF_PKTLEN(context->om);
    if (receivedSize > MAX_RECEIVED_MESSAGE_SIZE) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t received[MAX_RECEIVED_MESSAGE_SIZE];
    uint16_t copiedSize = receivedSize;
    const int result = ble_hs_mbuf_to_flat(
        context->om, received, sizeof(received), &copiedSize);
    if (result != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    for (uint16_t index = 0; index < copiedSize; ++index) {
        if (received[index] > 0x7f) {
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    uint8_t response[(sizeof(ECHO_PREFIX) - 1) + MAX_RECEIVED_MESSAGE_SIZE];
    memcpy(response, ECHO_PREFIX, sizeof(ECHO_PREFIX) - 1);
    memcpy(response + sizeof(ECHO_PREFIX) - 1, received, copiedSize);
    sendSerialBytes(response, sizeof(ECHO_PREFIX) - 1 + copiedSize);
    return 0;
}

static int accessTransmitData(uint16_t,
                              uint16_t,
                              ble_gatt_access_ctxt *,
                              void *) {
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

static const ble_gatt_chr_def characteristics[] = {
    {
        .uuid = &SERIAL_TX_UUID.u,
        .access_cb = accessTransmitData,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &transmitValueHandle,
    },
    {
        .uuid = &SERIAL_RX_UUID.u,
        .access_cb = receiveSerialData,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    {},
};

static const ble_gatt_svc_def services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SERIAL_SERVICE_UUID.u,
        .characteristics = characteristics,
    },
    {},
};

static int handleGapEvent(ble_gap_event *event, void *) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                portENTER_CRITICAL(&connectionStateLock);
                connectionHandle = event->connect.conn_handle;
                notificationsEnabled = false;
                portEXIT_CRITICAL(&connectionStateLock);
            } else {
                startAdvertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            portENTER_CRITICAL(&connectionStateLock);
            connectionHandle = INVALID_CONNECTION_HANDLE;
            notificationsEnabled = false;
            portEXIT_CRITICAL(&connectionStateLock);
            startAdvertising();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            startAdvertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == transmitValueHandle) {
                portENTER_CRITICAL(&connectionStateLock);
                notificationsEnabled = event->subscribe.cur_notify != 0;
                portEXIT_CRITICAL(&connectionStateLock);
            }
            break;

        default:
            break;
    }

    return 0;
}

static void startAdvertising() {
    ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = const_cast<ble_uuid128_t *>(&SERIAL_SERVICE_UUID);
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int result = ble_gap_adv_set_fields(&fields);
    if (result != 0) {
        printf("Failed to set BLE advertising fields: %d\n", result);
        return;
    }

    ble_hs_adv_fields scanResponse = {};
    scanResponse.name = reinterpret_cast<const uint8_t *>(ADVERTISED_NAME);
    scanResponse.name_len = sizeof(ADVERTISED_NAME) - 1;
    scanResponse.name_is_complete = 0;
    result = ble_gap_adv_rsp_set_fields(&scanResponse);
    if (result != 0) {
        printf("Failed to set BLE scan response: %d\n", result);
        return;
    }

    ble_gap_adv_params parameters = {};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    result = ble_gap_adv_start(ownAddressType, nullptr, BLE_HS_FOREVER,
                               &parameters, handleGapEvent, nullptr);
    if (result != 0) {
        printf("Failed to start BLE advertising: %d\n", result);
    }
}

static void onNimbleReset(int reason) {
    printf("NimBLE host reset, reason: %d\n", reason);
}

static void onNimbleSynchronized() {
    const int result = ble_hs_id_infer_auto(0, &ownAddressType);
    if (result != 0) {
        printf("Failed to determine BLE address type: %d\n", result);
        return;
    }
    startAdvertising();
}

static void runNimbleHost(void *) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void publishIdleMessage(void *) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        sendSerialBytes(reinterpret_cast<const uint8_t *>(IDLE_MESSAGE),
                        sizeof(IDLE_MESSAGE) - 1);
    }
}

static void blinkStatusLed(void *) {
    ESP_ERROR_CHECK(gpio_reset_pin(STATUS_LED_GPIO));
    ESP_ERROR_CHECK(gpio_set_direction(STATUS_LED_GPIO, GPIO_MODE_OUTPUT));

    uint32_t ledLevel = 0;
    ESP_ERROR_CHECK(gpio_set_level(STATUS_LED_GPIO, ledLevel));
    while (true) {
        vTaskDelay(STATUS_LED_INTERVAL);
        ledLevel = ledLevel == 0 ? 1 : 0;
        ESP_ERROR_CHECK(gpio_set_level(STATUS_LED_GPIO, ledLevel));
    }
}

extern "C" void app_main() {
    const BaseType_t blinkTaskCreated = xTaskCreatePinnedToCore(
        blinkStatusLed, "status-led", 2048, nullptr, 4, nullptr, 0);
    if (blinkTaskCreated != pdPASS) {
        printf("Failed to create status LED task\n");
    }

    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    transmitMutex = xSemaphoreCreateMutex();
    if (transmitMutex == nullptr) {
        printf("Failed to create BLE transmit mutex\n");
        return;
    }
    ESP_ERROR_CHECK(nimble_port_init());

    ble_hs_cfg.reset_cb = onNimbleReset;
    ble_hs_cfg.sync_cb = onNimbleSynchronized;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(DEVICE_NAME));
    ESP_ERROR_CHECK(ble_gatts_count_cfg(services));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(services));

    nimble_port_freertos_init(runNimbleHost);
    const BaseType_t taskCreated = xTaskCreatePinnedToCore(
        publishIdleMessage, "ble-idle", 3072, nullptr, 5, nullptr,
        BLE_IDLE_TASK_CORE);
    if (taskCreated != pdPASS) {
        printf("Failed to create BLE idle task\n");
    }
}
