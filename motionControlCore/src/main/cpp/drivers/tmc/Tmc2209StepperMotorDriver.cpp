#include "drivers/tmc/Tmc2209StepperMotorDriver.h"

Tmc2209StepperMotorDriver::Tmc2209StepperMotorDriver(
    StepperMotorDriver& stepDirectionDriver,
    UartSerial& uartSerial,
    uint8_t nodeAddress)
    : m_stepDirectionDriver(&stepDirectionDriver),
      m_uartDriver(uartSerial, nodeAddress)
{
}

bool
Tmc2209StepperMotorDriver::initialize()
{
    return m_stepDirectionDriver->initialize();
}

void
Tmc2209StepperMotorDriver::setDirection(bool forward)
{
    m_stepDirectionDriver->setDirection(forward);
}

void
Tmc2209StepperMotorDriver::pulseStep()
{
    m_stepDirectionDriver->pulseStep();
}

void
Tmc2209StepperMotorDriver::waitMicroseconds(uint32_t microseconds)
{
    m_stepDirectionDriver->waitMicroseconds(microseconds);
}

void
Tmc2209StepperMotorDriver::configureUart(uint32_t baudRate)
{
    m_uartDriver.initialize(baudRate);
}

void
Tmc2209StepperMotorDriver::writeGlobalConfig(
    const Tmc2209GlobalConfig& config)
{
    m_uartDriver.writeGlobalConfig(config);
}

void
Tmc2209StepperMotorDriver::writeCurrentControl(
    const Tmc2209CurrentControl& control)
{
    m_uartDriver.writeCurrentControl(control);
}

void
Tmc2209StepperMotorDriver::writeChopperConfig(
    const Tmc2209ChopperConfig& config)
{
    m_uartDriver.writeChopperConfig(config);
}

bool
Tmc2209StepperMotorDriver::readDriverStatus(Tmc2209DriverStatus& outStatus)
{
    return m_uartDriver.readDriverStatus(outStatus);
}
