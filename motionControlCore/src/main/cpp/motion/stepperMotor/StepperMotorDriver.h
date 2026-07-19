#ifndef MOTION_STEPPER_MOTOR_DRIVER_H
#define MOTION_STEPPER_MOTOR_DRIVER_H

class StepperMotorDriver {
  protected:
    ~StepperMotorDriver() = default;

  public:
    virtual bool initialize() = 0;
    virtual void setDirection(bool forward) = 0;
    virtual void pulseStep() = 0;
};

#endif
