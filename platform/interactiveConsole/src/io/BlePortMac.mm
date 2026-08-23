#include "BlePort.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

static NSString *const NUS_SERVICE_UUID =
    @"6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static NSString *const NUS_RX_UUID =
    @"6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static NSString *const NUS_TX_UUID =
    @"6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

class MacBlePort;

@interface MacBleDelegate : NSObject<CBCentralManagerDelegate, CBPeripheralDelegate>
- (instancetype)initWithOwner:(MacBlePort *)owner;
@end

class MacBlePort : public BlePort {
 public:
  MacBlePort()
      : queue_(dispatch_queue_create("motion-control-ble", DISPATCH_QUEUE_SERIAL)),
        delegate_([[MacBleDelegate alloc] initWithOwner:this]),
        manager_(nullptr),
        peripheral_(nullptr),
        rx_characteristic_(nullptr),
        open_(false),
        failed_(false),
        connection_lost_(false) {
    dispatch_sync(queue_, ^{
      manager_ = [[CBCentralManager alloc] initWithDelegate:delegate_ queue:queue_];
    });
  }

  ~MacBlePort() override {
    dispatch_sync(queue_, ^{
      if (manager_ != nullptr) {
        [manager_ stopScan];
        if (peripheral_ != nullptr) {
          [manager_ cancelPeripheralConnection:peripheral_];
        }
      }
    });
  }

  bool waitUntilReady(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool ready = condition_.wait_for(lock, timeout, [&]() {
      return open_ || failed_;
    });
    return ready && open_;
  }

  bool isOpen() const override {
    std::lock_guard<std::mutex> lock(mutex_);
    return open_;
  }

  std::string name() const override {
    std::lock_guard<std::mutex> lock(mutex_);
    return device_name_;
  }

  std::string readAvailable(bool &connection_lost) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string output;
    while (!received_.empty()) {
      output += received_.front();
      received_.pop_front();
    }
    connection_lost = connection_lost_;
    return output;
  }

  bool writeText(const std::string &text) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!open_ || peripheral_ == nullptr || rx_characteristic_ == nullptr) {
      return false;
    }

    NSData *data = [NSData dataWithBytes:text.data() length:text.size()];
    CBPeripheral *peripheral = peripheral_;
    CBCharacteristic *characteristic = rx_characteristic_;
    dispatch_async(queue_, ^{
      CBCharacteristicWriteType writeType =
          (characteristic.properties & CBCharacteristicPropertyWrite) ?
              CBCharacteristicWriteWithResponse :
              CBCharacteristicWriteWithoutResponse;
      [peripheral writeValue:data forCharacteristic:characteristic type:writeType];
    });
    return true;
  }

  void centralPoweredOn(CBCentralManager *manager) {
    CBUUID *service_uuid = [CBUUID UUIDWithString:NUS_SERVICE_UUID];
    [manager scanForPeripheralsWithServices:nil
                                    options:@{
                                      CBCentralManagerScanOptionAllowDuplicatesKey: @YES
                                    }];
    static_cast<void>(service_uuid);
  }

  void centralUnavailable() {
    std::lock_guard<std::mutex> lock(mutex_);
    failed_ = true;
    condition_.notify_all();
  }

  void discovered(
      CBCentralManager *manager,
      CBPeripheral *peripheral,
      NSDictionary<NSString *, id> *advertisementData) {
    NSString *local_name = advertisementData[CBAdvertisementDataLocalNameKey];
    NSString *name = local_name != nil ? local_name : peripheral.name;
    NSArray<CBUUID *> *service_uuids =
        advertisementData[CBAdvertisementDataServiceUUIDsKey];
    bool name_matches = false;
    if (name != nil) {
      name_matches =
          [name rangeOfString:@"Vitral" options:NSCaseInsensitiveSearch].location !=
              NSNotFound ||
          [name rangeOfString:@"ESP" options:NSCaseInsensitiveSearch].location !=
              NSNotFound ||
          [name rangeOfString:@"plotter" options:NSCaseInsensitiveSearch].location !=
              NSNotFound;
    }

    bool service_matches = false;
    CBUUID *nus_uuid = [CBUUID UUIDWithString:NUS_SERVICE_UUID];
    for (CBUUID *uuid in service_uuids) {
      if ([uuid isEqual:nus_uuid]) {
        service_matches = true;
      }
    }

    if (!name_matches && !service_matches) {
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (peripheral_ != nullptr) {
        return;
      }
      peripheral_ = peripheral;
      peripheral_.delegate = delegate_;
      if (name != nil) {
        device_name_ = [name UTF8String];
      }
    }
    [manager stopScan];
    [manager connectPeripheral:peripheral options:nil];
  }

  void connected(CBPeripheral *peripheral) {
    CBUUID *service_uuid = [CBUUID UUIDWithString:NUS_SERVICE_UUID];
    [peripheral discoverServices:@[ service_uuid ]];
  }

  void disconnected() {
    std::lock_guard<std::mutex> lock(mutex_);
    open_ = false;
    connection_lost_ = true;
    condition_.notify_all();
  }

  void servicesDiscovered(CBPeripheral *peripheral) {
    CBUUID *rx_uuid = [CBUUID UUIDWithString:NUS_RX_UUID];
    CBUUID *tx_uuid = [CBUUID UUIDWithString:NUS_TX_UUID];
    for (CBService *service in peripheral.services) {
      if ([service.UUID isEqual:[CBUUID UUIDWithString:NUS_SERVICE_UUID]]) {
        [peripheral discoverCharacteristics:@[ rx_uuid, tx_uuid ] forService:service];
      }
    }
  }

  void characteristicsDiscovered(CBPeripheral *peripheral, CBService *service) {
    for (CBCharacteristic *characteristic in service.characteristics) {
      if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:NUS_RX_UUID]]) {
        std::lock_guard<std::mutex> lock(mutex_);
        rx_characteristic_ = characteristic;
      }
      if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:NUS_TX_UUID]]) {
        [peripheral setNotifyValue:YES forCharacteristic:characteristic];
      }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (rx_characteristic_ != nullptr) {
      open_ = true;
      condition_.notify_all();
    }
  }

  void valueUpdated(CBCharacteristic *characteristic) {
    NSData *value = characteristic.value;
    if (value == nil || value.length == 0) {
      return;
    }

    std::string text(
        static_cast<const char *>(value.bytes),
        static_cast<size_t>(value.length));
    std::lock_guard<std::mutex> lock(mutex_);
    received_.push_back(text);
  }

 private:
  dispatch_queue_t queue_;
  MacBleDelegate *delegate_;
  CBCentralManager *manager_;
  CBPeripheral *peripheral_;
  CBCharacteristic *rx_characteristic_;

  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::deque<std::string> received_;
  std::string device_name_;
  bool open_;
  bool failed_;
  bool connection_lost_;
};

@implementation MacBleDelegate {
  MacBlePort *owner_;
}

- (instancetype)initWithOwner:(MacBlePort *)owner {
  self = [super init];
  if (self != nil) {
    owner_ = owner;
  }
  return self;
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
  if (central.state == CBManagerStatePoweredOn) {
    owner_->centralPoweredOn(central);
  } else if (central.state != CBManagerStateUnknown &&
             central.state != CBManagerStateResetting) {
    owner_->centralUnavailable();
  }
}

- (void)centralManager:(CBCentralManager *)central
 didDiscoverPeripheral:(CBPeripheral *)peripheral
     advertisementData:(NSDictionary<NSString *,id> *)advertisementData
                  RSSI:(NSNumber *)RSSI {
  static_cast<void>(RSSI);
  owner_->discovered(central, peripheral, advertisementData);
}

- (void)centralManager:(CBCentralManager *)central
  didConnectPeripheral:(CBPeripheral *)peripheral {
  static_cast<void>(central);
  owner_->connected(peripheral);
}

- (void)centralManager:(CBCentralManager *)central
didDisconnectPeripheral:(CBPeripheral *)peripheral
                 error:(NSError *)error {
  static_cast<void>(central);
  static_cast<void>(peripheral);
  static_cast<void>(error);
  owner_->disconnected();
}

- (void)peripheral:(CBPeripheral *)peripheral
didDiscoverServices:(NSError *)error {
  static_cast<void>(error);
  owner_->servicesDiscovered(peripheral);
}

- (void)peripheral:(CBPeripheral *)peripheral
didDiscoverCharacteristicsForService:(CBService *)service
             error:(NSError *)error {
  static_cast<void>(error);
  owner_->characteristicsDiscovered(peripheral, service);
}

- (void)peripheral:(CBPeripheral *)peripheral
didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
             error:(NSError *)error {
  static_cast<void>(peripheral);
  static_cast<void>(error);
  owner_->valueUpdated(characteristic);
}

@end

std::unique_ptr<BlePort> BlePort::scanAndConnect(
    std::chrono::milliseconds timeout) {
  std::unique_ptr<MacBlePort> port(new MacBlePort());
  if (!port->waitUntilReady(timeout)) {
    return nullptr;
  }
  return port;
}
