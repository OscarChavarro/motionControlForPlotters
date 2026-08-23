import CoreBluetooth
import Foundation

private func log(_ text: String = "") -> Void {
    print(text)
    fflush(stdout)
}

private func logInline(_ text: String) -> Void {
    print(text, terminator: "")
    fflush(stdout)
}

private let serviceUUID = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
private let rxUUID = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
private let txUUID = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E")

final class BleScanner: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private var manager: CBCentralManager!
    private var targetPeripheral: CBPeripheral?
    private var rxCharacteristic: CBCharacteristic?
    private var inputSource: DispatchSourceRead?
    private var inputBuffer = Data()
    private var pendingWrites = [String]()
    private var discoveredIdentifiers = Set<UUID>()

    override init() {
        super.init()
        manager = CBCentralManager(delegate: self, queue: .main)
        log("Starting BLE scan tool.")
        startInteractiveInput()
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        guard central.state == .poweredOn else {
            log("BLE state: \(stateName(central.state))")
            if central.state != .unknown && central.state != .resetting {
                finish()
            }
            return
        }

        log("BLE state: poweredOn")
        log("Scanning for Vitral/ESP/Nordic UART devices...")
        prompt()
        central.scanForPeripherals(
            withServices: nil,
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
    }

    func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let localName = advertisementData[CBAdvertisementDataLocalNameKey] as? String
        let name = localName ?? peripheral.name ?? ""
        let advertisedUUIDs =
            (advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID] ?? []) +
            (advertisementData[CBAdvertisementDataOverflowServiceUUIDsKey] as? [CBUUID] ?? [])

        let nameMatches =
            name.range(of: "Vitral", options: .caseInsensitive) != nil ||
            name.range(of: "ESP", options: .caseInsensitive) != nil ||
            name.range(of: "plotter", options: .caseInsensitive) != nil
        let serviceMatches = advertisedUUIDs.contains(serviceUUID)

        guard nameMatches || serviceMatches else {
            return
        }

        if discoveredIdentifiers.insert(peripheral.identifier).inserted {
            clearPromptLine()
            log()
            log("DEVICE")
            log("  identifier: \(peripheral.identifier)")
            log("  name: \(name.isEmpty ? "(no name)" : name)")
            log("  rssi: \(RSSI)")
            log("  matched: \(serviceMatches ? "Nordic UART Service" : "name")")
            log("  advertisement:")
            for key in advertisementData.keys.sorted() {
                log("    \(key): \(formatAdvertisementValue(advertisementData[key]!))")
            }
        }

        guard targetPeripheral == nil else {
            return
        }

        targetPeripheral = peripheral
        peripheral.delegate = self
        log()
        log("Connecting to \(name.isEmpty ? peripheral.identifier.uuidString : name)...")
        prompt()
        central.stopScan()
        central.connect(peripheral, options: nil)
    }

    func centralManager(
        _ central: CBCentralManager,
        didConnect peripheral: CBPeripheral
    ) {
        clearPromptLine()
        log("Connected.")
        prompt()
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(
        _ central: CBCentralManager,
        didFailToConnect peripheral: CBPeripheral,
        error: Error?
    ) {
        clearPromptLine()
        log("Connection failed: \(errorDescription(error))")
        prompt()
        targetPeripheral = nil
        central.scanForPeripherals(
            withServices: nil,
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
    }

    func centralManager(
        _ central: CBCentralManager,
        didDisconnectPeripheral peripheral: CBPeripheral,
        error: Error?
    ) {
        clearPromptLine()
        log("Disconnected: \(errorDescription(error))")
        prompt()
        targetPeripheral = nil
        rxCharacteristic = nil
        if central.state == .poweredOn {
            central.scanForPeripherals(
                withServices: nil,
                options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error {
            clearPromptLine()
            log("Service discovery failed: \(error.localizedDescription)")
            prompt()
            return
        }

        for service in peripheral.services ?? [] where service.uuid == serviceUUID {
            clearPromptLine()
            log("Discovered NUS service.")
            prompt()
            peripheral.discoverCharacteristics([rxUUID, txUUID], for: service)
        }
    }

    func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        if let error {
            clearPromptLine()
            log("Characteristic discovery failed: \(error.localizedDescription)")
            prompt()
            return
        }

        for characteristic in service.characteristics ?? [] {
            if characteristic.uuid == rxUUID {
                rxCharacteristic = characteristic
                clearPromptLine()
                log("Discovered RX write characteristic.")
                prompt()
                flushPendingWrites()
            }
            if characteristic.uuid == txUUID {
                clearPromptLine()
                log("Discovered TX notify characteristic.")
                prompt()
                peripheral.setNotifyValue(true, for: characteristic)
            }
        }
    }

    func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateNotificationStateFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        if let error {
            clearPromptLine()
            log("Notification subscribe failed: \(error.localizedDescription)")
            prompt()
            return
        }

        if characteristic.uuid == txUUID {
            clearPromptLine()
            log("TX notifications: \(characteristic.isNotifying ? "enabled" : "disabled")")
            prompt()
        }
    }

    func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        if let error {
            clearPromptLine()
            log("Notification read failed: \(error.localizedDescription)")
            prompt()
            return
        }

        guard characteristic.uuid == txUUID, let data = characteristic.value else {
            return
        }

        clearPromptLine()
        if let text = String(data: data, encoding: .utf8) {
            if text.hasSuffix("\n") {
                logInline("BLE RX: \(text)")
            }
            else {
                log("BLE RX: \(text)")
            }
        }
        else {
            log("BLE RX HEX: \(formatData(data))")
        }
        prompt()
    }

    private func finish() {
        inputSource?.cancel()
        inputSource = nil
        manager.stopScan()
        if let peripheral = targetPeripheral {
            manager.cancelPeripheralConnection(peripheral)
        }
        log()
        log("Scan finished. Interesting devices: \(discoveredIdentifiers.count)")
        exit(0)
    }

    private func startInteractiveInput() {
        let source = DispatchSource.makeReadSource(
            fileDescriptor: STDIN_FILENO,
            queue: .main)
        source.setEventHandler { [weak self] in
            self?.readAvailableInput()
        }
        source.setCancelHandler {
        }
        inputSource = source
        source.resume()
    }

    private func readAvailableInput() {
        var buffer = [UInt8](repeating: 0, count: 1024)
        let count = read(STDIN_FILENO, &buffer, buffer.count)

        if count == 0 {
            clearPromptLine()
            log("Input closed.")
            finish()
            return
        }

        if count < 0 {
            clearPromptLine()
            log("Input read failed.")
            finish()
            return
        }

        inputBuffer.append(buffer, count: count)
        processInputLines()
    }

    private func processInputLines() {
        while let newlineIndex = inputBuffer.firstIndex(of: 10) {
            let lineData = inputBuffer[..<newlineIndex]
            inputBuffer.removeSubrange(...newlineIndex)

            var line = String(data: lineData, encoding: .utf8) ?? ""
            if line.hasSuffix("\r") {
                line.removeLast()
            }

            if line == "exit" {
                clearPromptLine()
                log("Exiting.")
                finish()
                return
            }

            if !line.isEmpty {
                sendTextLine(line)
            }
            else {
                prompt()
            }
        }
    }

    private func sendTextLine(_ line: String) {
        guard
            let peripheral = targetPeripheral,
            let characteristic = rxCharacteristic,
            let data = "\(line)\n".data(using: .utf8)
        else {
            pendingWrites.append(line)
            clearPromptLine()
            log("Queued BLE TX until RX characteristic is ready: \(line)")
            prompt()
            return
        }

        let writeType: CBCharacteristicWriteType =
            characteristic.properties.contains(.write) ? .withResponse : .withoutResponse
        peripheral.writeValue(data, for: characteristic, type: writeType)
        clearPromptLine()
        log("BLE TX: \(line)")
        prompt()
    }

    private func flushPendingWrites() {
        guard !pendingWrites.isEmpty else {
            return
        }

        let writes = pendingWrites
        pendingWrites.removeAll()
        for line in writes {
            sendTextLine(line)
        }
    }
}

private func prompt() {
    logInline("ble> ")
}

private func clearPromptLine() {
    logInline("\r")
}

private func stateName(_ state: CBManagerState) -> String {
    switch state {
    case .unknown:
        return "unknown"
    case .resetting:
        return "resetting"
    case .unsupported:
        return "unsupported"
    case .unauthorized:
        return "unauthorized"
    case .poweredOff:
        return "poweredOff"
    case .poweredOn:
        return "poweredOn"
    @unknown default:
        return "unrecognized"
    }
}

private func errorDescription(_ error: Error?) -> String {
    error?.localizedDescription ?? "no error"
}

private func formatAdvertisementValue(_ value: Any) -> String {
    if let data = value as? Data {
        return formatData(data)
    }
    if let uuids = value as? [CBUUID] {
        return uuids.map { $0.uuidString }.joined(separator: ", ")
    }
    return String(describing: value)
}

private func formatData(_ data: Data) -> String {
    data.map { String(format: "%02x", $0) }.joined(separator: " ")
}

let scanner = BleScanner()
RunLoop.main.run()
