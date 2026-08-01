#ifndef MOTION_STEPPER_MOTOR_CONTROLLER_H
#define MOTION_STEPPER_MOTOR_CONTROLLER_H

#include <stdint.h>

class StepperMotorDriver;

class StepperMotorController {
  private:
    void setDirection(bool forward);
    void emitStep();
    uint32_t profileMilliStepsPerSecond(
        uint32_t cycleElapsedMilliseconds) const;
    uint64_t profileIntegratedMilliStepsMilliseconds(
        uint32_t cycleElapsedMilliseconds) const;
    uint32_t profileCycleDurationMilliseconds() const;

    StepperMotorDriver* m_driver;
    uint32_t m_maxMilliStepsPerSecond;
    uint16_t m_accelerationMilliseconds;
    uint16_t m_cruiseMilliseconds;
    uint16_t m_decelerationMilliseconds;
    uint32_t m_profileStartMilliseconds;
    uint32_t m_lastUpdateMilliseconds;
    uint64_t m_stepAccumulator;
    int32_t m_position;
    uint32_t m_speedMilliStepsPerSecond;
    bool m_directionForward;
    bool m_profileInitialized;

  public:
    StepperMotorController();

    bool initialize(StepperMotorDriver& driver);
    void configureRepeatingTrapezoid(
        uint32_t maxMilliStepsPerSecond,
        uint16_t accelerationMilliseconds,
        uint16_t cruiseMilliseconds,
        uint16_t decelerationMilliseconds);
    void update(uint32_t nowMilliseconds, bool enabled);

    // Immediately emits abs(steps) pulses (forward if steps > 0, backward
    // if steps < 0) at the given rate, blocking until done. Bypasses the
    // trapezoid profile; callers must keep the repeating test movement
    // disabled while using this, since it does not run through update().
    // stepsPerSecond must be > 0; pacing is done with
    // StepperMotorDriver::waitMicroseconds() between pulses, since this
    // is a one-shot blocking move rather than a scheduled profile.
    void moveBlockingSteps(int32_t steps, uint32_t stepsPerSecond);

    int32_t position() const;
    uint32_t speedMilliStepsPerSecond() const;
    bool directionForward() const;
};

#endif
