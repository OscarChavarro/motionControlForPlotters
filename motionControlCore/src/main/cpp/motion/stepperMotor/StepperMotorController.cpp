#include "motion/stepperMotor/StepperMotorController.h"

#include "motion/stepperMotor/StepperMotorDriver.h"

StepperMotorController::StepperMotorController()
    : m_driver(nullptr),
      m_maxMilliStepsPerSecond(0),
      m_accelerationMilliseconds(0),
      m_cruiseMilliseconds(0),
      m_decelerationMilliseconds(0),
      m_profileStartMilliseconds(0),
      m_lastUpdateMilliseconds(0),
      m_stepAccumulator(0),
      m_position(0),
      m_speedMilliStepsPerSecond(0),
      m_directionForward(false),
      m_profileInitialized(false)
{
}

bool
StepperMotorController::initialize(StepperMotorDriver& driver)
{
    m_driver = &driver;
    m_directionForward = false;
    m_profileInitialized = false;
    return m_driver->initialize();
}

void
StepperMotorController::configureRepeatingTrapezoid(
    uint32_t maxMilliStepsPerSecond,
    uint16_t accelerationMilliseconds,
    uint16_t cruiseMilliseconds,
    uint16_t decelerationMilliseconds)
{
    m_maxMilliStepsPerSecond = maxMilliStepsPerSecond;
    m_accelerationMilliseconds = accelerationMilliseconds;
    m_cruiseMilliseconds = cruiseMilliseconds;
    m_decelerationMilliseconds = decelerationMilliseconds;
    m_profileStartMilliseconds = 0;
    m_lastUpdateMilliseconds = 0;
    m_stepAccumulator = 0;
    m_speedMilliStepsPerSecond = 0;
    m_directionForward = false;
    m_profileInitialized = false;
}

void
StepperMotorController::update(uint32_t nowMilliseconds, bool enabled)
{
    if (!enabled || m_driver == nullptr ||
        profileCycleDurationMilliseconds() == 0UL) {
        m_lastUpdateMilliseconds = nowMilliseconds;
        m_stepAccumulator = 0;
        m_speedMilliStepsPerSecond = 0;
        m_profileInitialized = false;
        return;
    }

    if (!m_profileInitialized) {
        m_profileStartMilliseconds = nowMilliseconds;
        m_lastUpdateMilliseconds = nowMilliseconds;
        m_stepAccumulator = 0;
        m_profileInitialized = true;
    }

    const uint32_t elapsedMilliseconds =
        nowMilliseconds - m_lastUpdateMilliseconds;
    const uint32_t cycleDurationMilliseconds =
        profileCycleDurationMilliseconds();
    uint32_t profileElapsedMilliseconds =
        m_lastUpdateMilliseconds - m_profileStartMilliseconds;
    uint32_t unprocessedMilliseconds = elapsedMilliseconds;

    while (unprocessedMilliseconds > 0UL) {
        const uint32_t cycleIndex =
            profileElapsedMilliseconds / cycleDurationMilliseconds;
        const uint32_t cycleElapsedMilliseconds =
            profileElapsedMilliseconds % cycleDurationMilliseconds;
        const uint32_t millisecondsUntilCycleEnd =
            cycleDurationMilliseconds - cycleElapsedMilliseconds;
        const uint32_t intervalMilliseconds =
            unprocessedMilliseconds < millisecondsUntilCycleEnd ?
                unprocessedMilliseconds : millisecondsUntilCycleEnd;

        setDirection((cycleIndex % 2UL) == 0UL);
        m_stepAccumulator +=
            profileIntegratedMilliStepsMilliseconds(
                cycleElapsedMilliseconds + intervalMilliseconds) -
            profileIntegratedMilliStepsMilliseconds(
                cycleElapsedMilliseconds);

        while (m_stepAccumulator >= 1000000UL) {
            m_stepAccumulator -= 1000000UL;
            emitStep();
        }

        profileElapsedMilliseconds += intervalMilliseconds;
        unprocessedMilliseconds -= intervalMilliseconds;
        if (intervalMilliseconds == millisecondsUntilCycleEnd) {
            m_stepAccumulator = 0UL;
        }
    }

    m_lastUpdateMilliseconds = nowMilliseconds;
    const uint32_t currentCycleIndex =
        profileElapsedMilliseconds / cycleDurationMilliseconds;
    const uint32_t currentCycleElapsedMilliseconds =
        profileElapsedMilliseconds % cycleDurationMilliseconds;
    setDirection((currentCycleIndex % 2UL) == 0UL);
    m_speedMilliStepsPerSecond =
        profileMilliStepsPerSecond(currentCycleElapsedMilliseconds);
}

int32_t
StepperMotorController::position() const
{
    return m_position;
}

uint32_t
StepperMotorController::speedMilliStepsPerSecond() const
{
    return m_speedMilliStepsPerSecond;
}

bool
StepperMotorController::directionForward() const
{
    return m_directionForward;
}

void
StepperMotorController::setDirection(bool forward)
{
    if (m_directionForward == forward) {
        return;
    }
    m_directionForward = forward;
    m_driver->setDirection(forward);
}

void
StepperMotorController::emitStep()
{
    m_driver->pulseStep();
    if (m_directionForward) {
        ++m_position;
    }
    else {
        --m_position;
    }
}

uint32_t
StepperMotorController::profileMilliStepsPerSecond(
    uint32_t cycleElapsedMilliseconds) const
{
    if (m_accelerationMilliseconds == 0U) {
        if (cycleElapsedMilliseconds == 0UL) {
            return m_maxMilliStepsPerSecond;
        }
    }
    if (cycleElapsedMilliseconds < m_accelerationMilliseconds) {
        return static_cast<uint32_t>(
            (static_cast<uint64_t>(m_maxMilliStepsPerSecond) *
                cycleElapsedMilliseconds) /
            m_accelerationMilliseconds);
    }

    const uint32_t cruiseEndMilliseconds =
        static_cast<uint32_t>(m_accelerationMilliseconds) +
        m_cruiseMilliseconds;
    if (cycleElapsedMilliseconds < cruiseEndMilliseconds) {
        return m_maxMilliStepsPerSecond;
    }

    const uint32_t decelerationElapsedMilliseconds =
        cycleElapsedMilliseconds - cruiseEndMilliseconds;
    if (m_decelerationMilliseconds == 0U ||
        decelerationElapsedMilliseconds >= m_decelerationMilliseconds) {
        return 0;
    }

    return static_cast<uint32_t>(
        (static_cast<uint64_t>(m_maxMilliStepsPerSecond) *
            (m_decelerationMilliseconds - decelerationElapsedMilliseconds)) /
        m_decelerationMilliseconds);
}

uint64_t
StepperMotorController::profileIntegratedMilliStepsMilliseconds(
    uint32_t cycleElapsedMilliseconds) const
{
    const uint32_t accelerationEnd = m_accelerationMilliseconds;
    const uint32_t cruiseEnd = accelerationEnd + m_cruiseMilliseconds;
    const uint32_t cycleEnd = cruiseEnd + m_decelerationMilliseconds;
    const uint32_t elapsed =
        cycleElapsedMilliseconds < cycleEnd ?
            cycleElapsedMilliseconds : cycleEnd;
    uint64_t integrated = 0ULL;

    if (m_accelerationMilliseconds > 0U) {
        const uint32_t accelerationElapsed =
            elapsed < accelerationEnd ? elapsed : accelerationEnd;
        integrated +=
            static_cast<uint64_t>(m_maxMilliStepsPerSecond) *
            accelerationElapsed * accelerationElapsed /
            (2ULL * m_accelerationMilliseconds);
    }

    if (elapsed > accelerationEnd) {
        const uint32_t cruiseElapsed =
            (elapsed < cruiseEnd ? elapsed : cruiseEnd) - accelerationEnd;
        integrated +=
            static_cast<uint64_t>(m_maxMilliStepsPerSecond) * cruiseElapsed;
    }

    if (elapsed > cruiseEnd && m_decelerationMilliseconds > 0U) {
        const uint32_t decelerationElapsed = elapsed - cruiseEnd;
        integrated +=
            static_cast<uint64_t>(m_maxMilliStepsPerSecond) *
            decelerationElapsed;
        integrated -=
            static_cast<uint64_t>(m_maxMilliStepsPerSecond) *
            decelerationElapsed * decelerationElapsed /
            (2ULL * m_decelerationMilliseconds);
    }

    return integrated;
}

uint32_t
StepperMotorController::profileCycleDurationMilliseconds() const
{
    return static_cast<uint32_t>(m_accelerationMilliseconds) +
        m_cruiseMilliseconds + m_decelerationMilliseconds;
}
