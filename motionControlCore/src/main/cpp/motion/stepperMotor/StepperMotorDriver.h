#ifndef MOTION_STEPPER_MOTOR_DRIVER_H
#define MOTION_STEPPER_MOTOR_DRIVER_H

#include <stdint.h>

class StepperMotorDriver {
  protected:
    ~StepperMotorDriver() = default;

  public:
    virtual bool initialize() = 0;
    virtual void setDirection(bool forward) = 0;
    virtual void pulseStep() = 0;

    // Blocks for at least the given number of microseconds. Used to pace
    // a blocking, speed-controlled move (see
    // StepperMotorController::moveBlockingSteps) when there is no
    // interrupt-driven timer fine-grained enough to schedule it
    // asynchronously on this platform.
    virtual void waitMicroseconds(uint32_t microseconds) = 0;
};

#endif
