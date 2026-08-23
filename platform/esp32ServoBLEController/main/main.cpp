#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static constexpr char DEVICE_NAME[] = "ESP-WROOM-32";
static constexpr char ADVERTISED_NAME[] = "ESP-WROOM-32";
static constexpr char BOOT_MESSAGE[] = "firmware boot\r\n";
static constexpr uint16_t INVALID_CONNECTION_HANDLE = 0xffff;
static constexpr size_t MAX_RECEIVED_MESSAGE_SIZE = 512;
static constexpr size_t COMMAND_BUFFER_SIZE = 64;
static constexpr gpio_num_t STATUS_LED_GPIO = GPIO_NUM_2;
static constexpr gpio_num_t MOTOR_PWM_GPIO = GPIO_NUM_22;
static constexpr ledc_mode_t MOTOR_PWM_MODE = LEDC_LOW_SPEED_MODE;
static constexpr ledc_timer_t MOTOR_PWM_TIMER = LEDC_TIMER_0;
static constexpr ledc_channel_t MOTOR_PWM_CHANNEL = LEDC_CHANNEL_0;
static constexpr ledc_timer_bit_t MOTOR_PWM_RESOLUTION = LEDC_TIMER_14_BIT;
static constexpr uint32_t MOTOR_PWM_FREQUENCY_HZ = 50;
static constexpr uint32_t MOTOR_PWM_FRAME_MICROSECONDS = 1000000UL /
    MOTOR_PWM_FREQUENCY_HZ;
static constexpr uint32_t MOTOR_PWM_MAX_DUTY =
    (1UL << static_cast<uint32_t>(MOTOR_PWM_RESOLUTION)) - 1UL;
static constexpr uint32_t SERVO_MIN_PULSE_MICROSECONDS = 1000;
static constexpr uint32_t SERVO_MAX_PULSE_MICROSECONDS = 2000;
static constexpr uint16_t SERVO_ELEMENT_ID = 0;
static constexpr uint16_t LED_ELEMENT_ID = 1;
static constexpr TickType_t STATUS_LED_INTERVAL = pdMS_TO_TICKS(1000);

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
static bool bootMessagePending;
static uint8_t motorPosition;
static bool motorPositionIncreasing = true;
static bool servoTestEnabled = true;
static portMUX_TYPE connectionStateLock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t transmitMutex;
static SemaphoreHandle_t motorPositionMutex;

static char commandBuffer[COMMAND_BUFFER_SIZE];
static size_t commandLength;

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

static void sendSerialText(const char *text) {
    sendSerialBytes(
        reinterpret_cast<const uint8_t *>(text), strlen(text));
}

static void sendFormattedLine(const char *format, ...) {
    char line[96];
    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);

    if (written < 0) {
        return;
    }

    const size_t textLength = static_cast<size_t>(written);
    const size_t boundedLength =
        textLength < sizeof(line) ? textLength : sizeof(line) - 1;
    sendSerialBytes(reinterpret_cast<const uint8_t *>(line), boundedLength);
    sendSerialText("\r\n");
}

static uint8_t readMotorPosition() {
    uint8_t position = 0;
    if (xSemaphoreTake(motorPositionMutex, portMAX_DELAY) == pdTRUE) {
        position = motorPosition;
        xSemaphoreGive(motorPositionMutex);
    }
    return position;
}

static uint32_t servoPulseMicrosecondsForPosition(uint8_t position) {
    return SERVO_MIN_PULSE_MICROSECONDS +
        ((static_cast<uint32_t>(position) *
          (SERVO_MAX_PULSE_MICROSECONDS - SERVO_MIN_PULSE_MICROSECONDS)) /
         255UL);
}

static uint32_t servoDutyForPosition(uint8_t position) {
    return (servoPulseMicrosecondsForPosition(position) * MOTOR_PWM_MAX_DUTY) /
        MOTOR_PWM_FRAME_MICROSECONDS;
}

static esp_err_t writeServoPosition(uint8_t position) {
    esp_err_t result = ledc_set_duty(
        MOTOR_PWM_MODE, MOTOR_PWM_CHANNEL, servoDutyForPosition(position));
    if (result != ESP_OK) {
        printf("Failed to set motor PWM duty: %s\n", esp_err_to_name(result));
        return result;
    }

    result = ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CHANNEL);
    if (result != ESP_OK) {
        printf("Failed to update motor PWM duty: %s\n", esp_err_to_name(result));
    }
    return result;
}

static void printHardwareConfiguration() {
    sendSerialText("Hardware by pin:\r\n");
    sendSerialText(
        "[0] GPIO22: 50Hz PWM servo output, motorPosition 0-255 maps to 1000-2000us\r\n");
    sendSerialText("[1] GPIO2: status LED heartbeat output\r\n");
}

static void printTelemetry(uint16_t requestedId) {
    const uint8_t position = readMotorPosition();
    const uint32_t pulseMicroseconds =
        servoPulseMicrosecondsForPosition(position);

    if (requestedId == SERVO_ELEMENT_ID) {
        sendFormattedLine(
            "SERVO,%u,%lu,%u",
            static_cast<unsigned>(position),
            static_cast<unsigned long>(pulseMicroseconds),
            servoTestEnabled ? 1U : 0U);
        return;
    }

    if (requestedId == LED_ELEMENT_ID) {
        sendSerialText("LED,HEARTBEAT,1000\r\n");
        return;
    }

    sendFormattedLine(
        "MotorPosition: %u GPIO22 PWM servo pulse=%luus test=%u",
        static_cast<unsigned>(position),
        static_cast<unsigned long>(pulseMicroseconds),
        servoTestEnabled ? 1U : 0U);
}

static bool commandEquals(const char *command, const char *expected) {
    return strcmp(command, expected) == 0;
}

static bool commandStartsWith(
    const char *command, const char *prefix, const char *&remainder) {
    while (*prefix != '\0') {
        if (*command != *prefix) {
            return false;
        }
        ++command;
        ++prefix;
    }
    remainder = command;
    return true;
}

static bool parseUnsignedId(const char *text, uint16_t &value) {
    if (*text < '0' || *text > '9') {
        return false;
    }

    uint16_t result = 0U;
    while (*text >= '0' && *text <= '9') {
        result = static_cast<uint16_t>(
            result * 10U + static_cast<uint16_t>(*text - '0'));
        ++text;
    }

    if (*text != '\0') {
        return false;
    }

    value = result;
    return true;
}

static bool parseUnsignedToken(
    const char *text,
    uint16_t &value,
    const char *&afterValue) {
    if (*text < '0' || *text > '9') {
        return false;
    }

    uint16_t result = 0U;
    while (*text >= '0' && *text <= '9') {
        result = static_cast<uint16_t>(
            result * 10U + static_cast<uint16_t>(*text - '0'));
        ++text;
    }

    value = result;
    afterValue = text;
    return true;
}

static void setServoPosition(uint8_t position, bool testEnabled) {
    if (xSemaphoreTake(motorPositionMutex, portMAX_DELAY) == pdTRUE) {
        motorPosition = position;
        servoTestEnabled = testEnabled;
        writeServoPosition(motorPosition);
        xSemaphoreGive(motorPositionMutex);
    }
}

static void handleServoCommand(const char *arguments) {
    uint16_t id = 0U;
    const char *cursor = nullptr;
    if (!parseUnsignedToken(arguments, id, cursor) || *cursor != ' ') {
        sendSerialText("Usage: servo <id> <position>\r\n");
        return;
    }

    uint16_t position = 0U;
    if (!parseUnsignedId(cursor + 1, position)) {
        sendSerialText("Usage: servo <id> <position>\r\n");
        return;
    }

    if (id != SERVO_ELEMENT_ID) {
        sendSerialText("Unknown element id.\r\n");
        return;
    }

    if (position > 255U) {
        sendSerialText("Error: servo position must be 0..255.\r\n");
        return;
    }

    setServoPosition(static_cast<uint8_t>(position), false);
    sendFormattedLine(
        "Servo %u position %u.",
        static_cast<unsigned>(id),
        static_cast<unsigned>(position));
}

static void handleTestCommand(const char *arguments) {
    if (commandEquals(arguments, "off") ||
        commandEquals(arguments, "disable")) {
        if (xSemaphoreTake(motorPositionMutex, portMAX_DELAY) == pdTRUE) {
            servoTestEnabled = false;
            xSemaphoreGive(motorPositionMutex);
        }
        sendSerialText("Servo test disabled.\r\n");
        return;
    }

    if (commandEquals(arguments, "on") ||
        commandEquals(arguments, "enable")) {
        if (xSemaphoreTake(motorPositionMutex, portMAX_DELAY) == pdTRUE) {
            servoTestEnabled = true;
            xSemaphoreGive(motorPositionMutex);
        }
        sendSerialText("Servo test enabled.\r\n");
        return;
    }

    uint16_t id = 0U;
    const char *cursor = nullptr;
    if (parseUnsignedToken(arguments, id, cursor) &&
        id == SERVO_ELEMENT_ID && *cursor == ' ') {
        if (commandEquals(cursor + 1, "enable")) {
            if (xSemaphoreTake(motorPositionMutex, portMAX_DELAY) == pdTRUE) {
                servoTestEnabled = true;
                xSemaphoreGive(motorPositionMutex);
            }
            sendSerialText("Servo test enabled.\r\n");
            return;
        }
        if (commandEquals(cursor + 1, "disable") ||
            commandEquals(cursor + 1, "off")) {
            if (xSemaphoreTake(motorPositionMutex, portMAX_DELAY) == pdTRUE) {
                servoTestEnabled = false;
                xSemaphoreGive(motorPositionMutex);
            }
            sendSerialText("Servo test disabled.\r\n");
            return;
        }
    }

    sendSerialText("Usage: test off|on or test <id> enable|disable\r\n");
}

static void handleCommand(const char *command) {
    if (command[0] == '\0') {
        return;
    }

    if (commandEquals(command, "hardware")) {
        printHardwareConfiguration();
        return;
    }

    if (commandEquals(command, ".")) {
        printTelemetry(2U);
        return;
    }

    const char *idText = nullptr;
    if (commandStartsWith(command, ". ", idText)) {
        uint16_t id = 0U;
        if (parseUnsignedId(idText, id)) {
            printTelemetry(id);
        }
        else {
            sendSerialText("Usage: . [<id>]\r\n");
        }
        return;
    }

    const char *arguments = nullptr;
    if (commandStartsWith(command, "servo ", arguments)) {
        handleServoCommand(arguments);
        return;
    }

    if (commandStartsWith(command, "test ", arguments)) {
        handleTestCommand(arguments);
        return;
    }

    sendFormattedLine("Unknown command: %s", command);
}

static void processReceivedByte(uint8_t received) {
    if (received == '\r' || received == '\n') {
        commandBuffer[commandLength] = '\0';
        handleCommand(commandBuffer);
        commandLength = 0U;
        return;
    }

    if (received == '\b' || received == 127U) {
        if (commandLength > 0U) {
            --commandLength;
        }
        return;
    }

    if (received < 32U || received > 126U) {
        return;
    }

    if (commandLength + 1U < COMMAND_BUFFER_SIZE) {
        commandBuffer[commandLength++] = static_cast<char>(received);
    }
    else {
        sendSerialText("Command too long.\r\n");
        commandLength = 0U;
    }
}

static void processReceivedPacket(const uint8_t *received, uint16_t size) {
    bool hasLineEnding = false;
    for (uint16_t index = 0; index < size; ++index) {
        if (received[index] == '\r' || received[index] == '\n') {
            hasLineEnding = true;
        }
    }

    if (!hasLineEnding && commandLength == 0U && size < COMMAND_BUFFER_SIZE) {
        for (uint16_t index = 0; index < size; ++index) {
            if (received[index] < 32U || received[index] > 126U) {
                return;
            }
            commandBuffer[index] = static_cast<char>(received[index]);
        }
        commandBuffer[size] = '\0';
        handleCommand(commandBuffer);
        return;
    }

    for (uint16_t index = 0; index < size; ++index) {
        processReceivedByte(received[index]);
    }
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

    processReceivedPacket(received, copiedSize);
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
                bootMessagePending = true;
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
                const bool shouldSendBootMessage =
                    notificationsEnabled && bootMessagePending;
                if (shouldSendBootMessage) {
                    bootMessagePending = false;
                }
                portEXIT_CRITICAL(&connectionStateLock);
                if (shouldSendBootMessage) {
                    sendSerialText(BOOT_MESSAGE);
                }
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

static bool configureMotorPwm() {
    ledc_timer_config_t timerConfig = {};
    timerConfig.speed_mode = MOTOR_PWM_MODE;
    timerConfig.duty_resolution = MOTOR_PWM_RESOLUTION;
    timerConfig.timer_num = MOTOR_PWM_TIMER;
    timerConfig.freq_hz = MOTOR_PWM_FREQUENCY_HZ;
    timerConfig.clk_cfg = LEDC_AUTO_CLK;
    esp_err_t result = ledc_timer_config(&timerConfig);
    if (result != ESP_OK) {
        printf("Failed to configure motor PWM timer: %s\n",
               esp_err_to_name(result));
        return false;
    }

    ledc_channel_config_t channelConfig = {};
    channelConfig.gpio_num = MOTOR_PWM_GPIO;
    channelConfig.speed_mode = MOTOR_PWM_MODE;
    channelConfig.channel = MOTOR_PWM_CHANNEL;
    channelConfig.intr_type = LEDC_INTR_DISABLE;
    channelConfig.timer_sel = MOTOR_PWM_TIMER;
    channelConfig.duty = servoDutyForPosition(motorPosition);
    channelConfig.hpoint = 0;
    result = ledc_channel_config(&channelConfig);
    if (result != ESP_OK) {
        printf("Failed to configure motor PWM channel: %s\n",
               esp_err_to_name(result));
        return false;
    }

    return true;
}

static void advanceMotorPosition() {
    uint8_t nextPosition = motorPosition;
    if (motorPositionIncreasing) {
        if (nextPosition >= 224U) {
            nextPosition = 255U;
            motorPositionIncreasing = false;
        }
        else {
            nextPosition = static_cast<uint8_t>(nextPosition + 32U);
        }
    }
    else {
        if (nextPosition == 255U) {
            nextPosition = 224U;
        }
        else if (nextPosition <= 32U) {
            nextPosition = 0U;
            motorPositionIncreasing = true;
        }
        else {
            nextPosition = static_cast<uint8_t>(nextPosition - 32U);
        }
    }

    motorPosition = nextPosition;
    writeServoPosition(motorPosition);
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

static void runMotorPwmCycle(void *) {
    if (!configureMotorPwm()) {
        vTaskDelete(nullptr);
    }

    while (true) {
        vTaskDelay(STATUS_LED_INTERVAL);
        if (xSemaphoreTake(motorPositionMutex, portMAX_DELAY) == pdTRUE) {
            if (servoTestEnabled) {
                advanceMotorPosition();
            }
            xSemaphoreGive(motorPositionMutex);
        }
    }
}

extern "C" void app_main() {
    printf("esp32ServoBLEController boot\n");

    motorPositionMutex = xSemaphoreCreateMutex();
    if (motorPositionMutex == nullptr) {
        printf("Failed to create motor position mutex\n");
        return;
    }

    const BaseType_t blinkTaskCreated = xTaskCreatePinnedToCore(
        blinkStatusLed, "status-led", 2048, nullptr, 4, nullptr, 0);
    if (blinkTaskCreated != pdPASS) {
        printf("Failed to create status LED task\n");
    }

    const BaseType_t motorTaskCreated = xTaskCreatePinnedToCore(
        runMotorPwmCycle, "motor-pwm", 3072, nullptr, 4, nullptr, 0);
    if (motorTaskCreated != pdPASS) {
        printf("Failed to create motor PWM task\n");
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
}
