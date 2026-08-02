#ifndef MOTION_CONTROL_HAL_MOTOR_DRIVER_ENABLE_H
#define MOTION_CONTROL_HAL_MOTOR_DRIVER_ENABLE_H

// Controls the shared stepper driver enable line (one pin gating every
// motor driver at once, e.g. the CNC Shield's EN pin). true drives the
// motors; false disables them regardless of what each motor's own
// step/direction pins are doing.
class MotorDriverEnable {
  protected:
    ~MotorDriverEnable() = default;

  public:
    virtual void initialize() = 0;
    virtual void setEnabled(bool enabled) = 0;
};

#endif
