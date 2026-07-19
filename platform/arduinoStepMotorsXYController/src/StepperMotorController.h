#ifndef STEPPER_MOTOR_CONTROLLER_H
#define STEPPER_MOTOR_CONTROLLER_H

#include <stdint.h>

class StepperMotorController {
  private:
    void setDirection(bool forward);
    void pulseStep();
    void emitStep();
    uint16_t profileSpeed(uint32_t cycleElapsedMilliseconds) const;
    uint32_t profileCycleDurationMilliseconds() const;

    static void delayMicroseconds(uint16_t microseconds);
    static void configureOutputPin(uint8_t pin);
    static void writeOutputPin(uint8_t pin, bool high);

    uint8_t m_stepPin;
    uint8_t m_directionPin;
    uint16_t m_stepPulseMicroseconds;
    uint16_t m_directionSetupMicroseconds;
    uint16_t m_maxStepsPerSecond;
    uint16_t m_accelerationMilliseconds;
    uint16_t m_cruiseMilliseconds;
    uint16_t m_decelerationMilliseconds;
    uint32_t m_profileStartMilliseconds;
    uint32_t m_lastUpdateMilliseconds;
    uint32_t m_stepAccumulator;
    int32_t m_position;
    uint16_t m_speedStepsPerSecond;
    bool m_directionForward;
    bool m_initialized;

  public:
    StepperMotorController(
        uint8_t stepPin,
        uint8_t directionPin,
        uint16_t stepPulseMicroseconds,
        uint16_t directionSetupMicroseconds);

    void initialize();
    void configureRepeatingTrapezoid(
        uint16_t maxStepsPerSecond,
        uint16_t accelerationMilliseconds,
        uint16_t cruiseMilliseconds,
        uint16_t decelerationMilliseconds);
    void update(uint32_t nowMilliseconds, bool enabled);

    int32_t position() const;
    uint16_t speedStepsPerSecond() const;
    bool directionForward() const;
};

#endif
