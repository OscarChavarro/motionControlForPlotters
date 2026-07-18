#include "StepperMotorController.h"

#include <avr/io.h>
#include <util/delay.h>

StepperMotorController::StepperMotorController(
    uint8_t stepPin,
    uint8_t directionPin,
    uint16_t stepPulseMicroseconds,
    uint16_t directionSetupMicroseconds)
    : m_stepPin(stepPin),
      m_directionPin(directionPin),
      m_stepPulseMicroseconds(stepPulseMicroseconds),
      m_directionSetupMicroseconds(directionSetupMicroseconds),
      m_maxStepsPerSecond(0),
      m_accelerationMilliseconds(0),
      m_cruiseMilliseconds(0),
      m_decelerationMilliseconds(0),
      m_profileStartMilliseconds(0),
      m_lastUpdateMilliseconds(0),
      m_stepAccumulator(0),
      m_position(0),
      m_speedStepsPerSecond(0),
      m_directionForward(false),
      m_initialized(false)
{
}

void StepperMotorController::initialize()
{
    configureOutputPin(m_stepPin);
    configureOutputPin(m_directionPin);
    writeOutputPin(m_stepPin, false);
    writeOutputPin(m_directionPin, false);
}

void StepperMotorController::configureRepeatingTrapezoid(
    uint16_t maxStepsPerSecond,
    uint16_t accelerationMilliseconds,
    uint16_t cruiseMilliseconds,
    uint16_t decelerationMilliseconds)
{
    m_maxStepsPerSecond = maxStepsPerSecond;
    m_accelerationMilliseconds = accelerationMilliseconds;
    m_cruiseMilliseconds = cruiseMilliseconds;
    m_decelerationMilliseconds = decelerationMilliseconds;
    m_profileStartMilliseconds = 0;
    m_lastUpdateMilliseconds = 0;
    m_stepAccumulator = 0;
    m_speedStepsPerSecond = 0;
    m_directionForward = false;
    m_initialized = false;
}

void StepperMotorController::update(uint32_t nowMilliseconds, bool enabled)
{
    if (!enabled || profileCycleDurationMilliseconds() == 0UL) {
        m_lastUpdateMilliseconds = nowMilliseconds;
        m_stepAccumulator = 0;
        m_speedStepsPerSecond = 0;
        m_initialized = false;
        return;
    }

    if (!m_initialized) {
        m_profileStartMilliseconds = nowMilliseconds;
        m_lastUpdateMilliseconds = nowMilliseconds;
        m_stepAccumulator = 0;
        m_initialized = true;
    }

    const uint32_t elapsedMilliseconds =
        nowMilliseconds - m_lastUpdateMilliseconds;
    if (elapsedMilliseconds == 0UL) {
        return;
    }

    m_lastUpdateMilliseconds = nowMilliseconds;

    const uint32_t cycleDurationMilliseconds =
        profileCycleDurationMilliseconds();
    const uint32_t profileElapsedMilliseconds =
        nowMilliseconds - m_profileStartMilliseconds;
    const uint32_t cycleIndex =
        profileElapsedMilliseconds / cycleDurationMilliseconds;
    const uint32_t cycleElapsedMilliseconds =
        profileElapsedMilliseconds % cycleDurationMilliseconds;

    setDirection((cycleIndex % 2UL) == 0UL);
    m_speedStepsPerSecond = profileSpeed(cycleElapsedMilliseconds);
    m_stepAccumulator +=
        static_cast<uint32_t>(m_speedStepsPerSecond) * elapsedMilliseconds;

    while (m_stepAccumulator >= 1000UL) {
        m_stepAccumulator -= 1000UL;
        emitStep();
    }
}

int32_t StepperMotorController::position() const
{
    return m_position;
}

uint16_t StepperMotorController::speedStepsPerSecond() const
{
    return m_speedStepsPerSecond;
}

bool StepperMotorController::directionForward() const
{
    return m_directionForward;
}

void StepperMotorController::setDirection(bool forward)
{
    if (m_directionForward == forward) {
        return;
    }
    m_directionForward = forward;
    writeOutputPin(m_directionPin, forward);
    delayMicroseconds(m_directionSetupMicroseconds);
}

void StepperMotorController::pulseStep()
{
    writeOutputPin(m_stepPin, true);
    delayMicroseconds(m_stepPulseMicroseconds);
    writeOutputPin(m_stepPin, false);
    delayMicroseconds(m_stepPulseMicroseconds);
}

void StepperMotorController::emitStep()
{
    pulseStep();
    if (m_directionForward) {
        ++m_position;
    }
    else {
        --m_position;
    }
}

uint16_t StepperMotorController::profileSpeed(
    uint32_t cycleElapsedMilliseconds) const
{
    if (m_accelerationMilliseconds == 0U) {
        if (cycleElapsedMilliseconds == 0UL) {
            return m_maxStepsPerSecond;
        }
    }
    if (cycleElapsedMilliseconds < m_accelerationMilliseconds) {
        return static_cast<uint16_t>(
            (static_cast<uint32_t>(m_maxStepsPerSecond) *
                cycleElapsedMilliseconds) /
            m_accelerationMilliseconds);
    }

    const uint32_t cruiseEndMilliseconds =
        static_cast<uint32_t>(m_accelerationMilliseconds) +
        m_cruiseMilliseconds;
    if (cycleElapsedMilliseconds < cruiseEndMilliseconds) {
        return m_maxStepsPerSecond;
    }

    const uint32_t decelerationElapsedMilliseconds =
        cycleElapsedMilliseconds - cruiseEndMilliseconds;
    if (m_decelerationMilliseconds == 0U) {
        return 0;
    }
    if (decelerationElapsedMilliseconds >= m_decelerationMilliseconds) {
        return 0;
    }

    return static_cast<uint16_t>(
        (static_cast<uint32_t>(m_maxStepsPerSecond) *
            (m_decelerationMilliseconds - decelerationElapsedMilliseconds)) /
        m_decelerationMilliseconds);
}

uint32_t StepperMotorController::profileCycleDurationMilliseconds() const
{
    return static_cast<uint32_t>(m_accelerationMilliseconds) +
        m_cruiseMilliseconds + m_decelerationMilliseconds;
}

void StepperMotorController::delayMicroseconds(uint16_t microseconds)
{
    for (uint16_t i = 0; i < microseconds; ++i) {
        _delay_us(1);
    }
}

void StepperMotorController::configureOutputPin(uint8_t pin)
{
#if defined(ARDUINO_AVR_MEGA2560)
    if (pin == 2U) {
        DDRE = static_cast<uint8_t>(DDRE | (1U << PE4));
    }
    else if (pin == 3U) {
        DDRE = static_cast<uint8_t>(DDRE | (1U << PE5));
    }
#elif defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO)
    if (pin == 2U) {
        DDRD = static_cast<uint8_t>(DDRD | (1U << PD2));
    }
    else if (pin == 3U) {
        DDRD = static_cast<uint8_t>(DDRD | (1U << PD3));
    }
#else
#error "StepperMotorController pin mapping is not defined for this Arduino board"
#endif
}

void StepperMotorController::writeOutputPin(uint8_t pin, bool high)
{
#if defined(ARDUINO_AVR_MEGA2560)
    if (pin == 2U) {
        if (high) {
            PORTE = static_cast<uint8_t>(PORTE | (1U << PE4));
        }
        else {
            PORTE = static_cast<uint8_t>(PORTE & ~(1U << PE4));
        }
    }
    else if (pin == 3U) {
        if (high) {
            PORTE = static_cast<uint8_t>(PORTE | (1U << PE5));
        }
        else {
            PORTE = static_cast<uint8_t>(PORTE & ~(1U << PE5));
        }
    }
#elif defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO)
    if (pin == 2U) {
        if (high) {
            PORTD = static_cast<uint8_t>(PORTD | (1U << PD2));
        }
        else {
            PORTD = static_cast<uint8_t>(PORTD & ~(1U << PD2));
        }
    }
    else if (pin == 3U) {
        if (high) {
            PORTD = static_cast<uint8_t>(PORTD | (1U << PD3));
        }
        else {
            PORTD = static_cast<uint8_t>(PORTD & ~(1U << PD3));
        }
    }
#else
#error "StepperMotorController pin mapping is not defined for this Arduino board"
#endif
}
