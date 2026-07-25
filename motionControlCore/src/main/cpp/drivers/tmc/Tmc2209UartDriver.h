#ifndef MOTION_CONTROL_DRIVERS_TMC_TMC2209_UART_DRIVER_H
#define MOTION_CONTROL_DRIVERS_TMC_TMC2209_UART_DRIVER_H

#include <stdint.h>

#include "drivers/tmc/Tmc2209Datagram.h"
#include "drivers/tmc/Tmc2209Registers.h"

class UartSerial;

// Single-wire UART driver for the TMC2209, built on top of the existing
// UartSerial HAL interface (the same abstraction used for the console).
// A TMC2209 exposes UART as one physical pin (PDN_UART); wiring it to a
// UartSerial implementation is the caller's responsibility (for example
// tying two Mega2560 USART pins to that single pin through a series
// resistor, as described in chapter 4.3 of the datasheet).
class Tmc2209UartDriver {
  private:
    static const uint8_t MAX_READ_POLL_ITERATIONS = 255U;

    UartSerial* m_serial;
    uint8_t m_nodeAddress;

    bool receiveReadReply(
        uint8_t datagram[Tmc2209Datagram::READ_REPLY_LENGTH]);

  public:
    Tmc2209UartDriver(UartSerial& serial, uint8_t nodeAddress);

    void initialize(uint32_t baudRate);

    void writeRegister(uint8_t registerAddress, uint32_t data);
    bool readRegister(uint8_t registerAddress, uint32_t& outData);

    void writeGlobalConfig(const Tmc2209GlobalConfig& config);
    void writeCurrentControl(const Tmc2209CurrentControl& control);
    void writeChopperConfig(const Tmc2209ChopperConfig& config);
    bool readDriverStatus(Tmc2209DriverStatus& outStatus);
};

#endif
