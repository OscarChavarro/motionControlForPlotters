#ifndef MOTION_CONTROL_DRIVERS_TMC_TMC2209_STEPPER_MOTOR_DRIVER_H
#define MOTION_CONTROL_DRIVERS_TMC_TMC2209_STEPPER_MOTOR_DRIVER_H

#include <stdint.h>

#include "drivers/tmc/Tmc2209Registers.h"
#include "drivers/tmc/Tmc2209UartDriver.h"
#include "motion/stepperMotor/StepperMotorDriver.h"

class UartSerial;

// Adds TMC2209 UART configuration on top of an existing STEP/DIR
// StepperMotorDriver, without changing how motion is driven.
//
// A TMC2209 keeps working through its STEP/DIR pins whether UART is
// enabled or not -- UART only adds register-based configuration (current,
// microstepping, diagnostics), it does not replace STEP/DIR pulsing. So
// this class delegates initialize()/setDirection()/pulseStep() unchanged
// to whatever driver already pulses those pins (the same
// AvrStepDirectionDriver used for a plain A4988, or a TMC2209 with UART
// disabled), and only adds the extra UART-based configuration surface.
//
// This is a decorator, not a subclass of the STEP/DIR driver: the two
// concerns (toggling STEP/DIR pins vs. talking TMC2209 UART registers)
// vary independently, so composing them keeps each concern replaceable on
// its own. StepperMotorController only ever sees the StepperMotorDriver
// interface and does not need to know which mode is active.
class Tmc2209StepperMotorDriver : public StepperMotorDriver {
  private:
    StepperMotorDriver* m_stepDirectionDriver;
    Tmc2209UartDriver m_uartDriver;

  public:
    Tmc2209StepperMotorDriver(
        StepperMotorDriver& stepDirectionDriver,
        UartSerial& uartSerial,
        uint8_t nodeAddress);

    bool initialize() override;
    void setDirection(bool forward) override;
    void pulseStep() override;
    void waitMicroseconds(uint32_t microseconds) override;

    void configureUart(uint32_t baudRate);
    void writeGlobalConfig(const Tmc2209GlobalConfig& config);
    void writeCurrentControl(const Tmc2209CurrentControl& control);
    void writeChopperConfig(const Tmc2209ChopperConfig& config);
    bool readDriverStatus(Tmc2209DriverStatus& outStatus);
};

#endif
