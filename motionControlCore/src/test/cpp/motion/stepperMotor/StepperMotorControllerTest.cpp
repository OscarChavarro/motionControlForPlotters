#include "motion/stepperMotor/StepperMotor.h"
#include "motion/stepperMotor/StepperMotorController.h"
#include "motion/stepperMotor/StepperMotorDriver.h"

#include <stdint.h>

class FakeStepperMotorDriver : public StepperMotorDriver {
  private:
    bool m_forward;
    uint32_t m_forwardPulses;
    uint32_t m_backwardPulses;

  public:
    FakeStepperMotorDriver()
        : m_forward(false), m_forwardPulses(0), m_backwardPulses(0)
    {
    }

    ~FakeStepperMotorDriver() = default;

    bool initialize() override
    {
        return true;
    }

    void setDirection(bool forward) override
    {
        m_forward = forward;
    }

    void pulseStep() override
    {
        if (m_forward) {
            ++m_forwardPulses;
        }
        else {
            ++m_backwardPulses;
        }
    }

    uint32_t forwardPulses() const
    {
        return m_forwardPulses;
    }

    uint32_t backwardPulses() const
    {
        return m_backwardPulses;
    }
};

static StepperMotor
exampleMotor()
{
    return StepperMotor(
        BIPOLAR_STEPPER_MOTOR,
        "Artillery D42HSA5401-23B",
        200U,
        BIPOLAR_STEPPER_MOTOR_MICROSTEP_16,
        3U,
        2U);
}

static void
configureExampleProfile(
    StepperMotorController& controller,
    const StepperMotor& motor)
{
    controller.configureRepeatingTrapezoid(
        motor.milliMicroStepsPerSecondForTrapezoidRotations(
            2U,
            1000U,
            8000U,
            1000U),
        1000U,
        8000U,
        1000U);
}

int
main()
{
    const StepperMotor motor = exampleMotor();
    FakeStepperMotorDriver firstDriver;
    FakeStepperMotorDriver secondDriver;
    StepperMotorController firstController;
    StepperMotorController secondController;

    if (!firstController.initialize(firstDriver) ||
        !secondController.initialize(secondDriver)) {
        return 1;
    }
    configureExampleProfile(firstController, motor);
    configureExampleProfile(secondController, motor);

    firstController.update(0UL, true);
    secondController.update(0UL, true);
    firstController.update(10000UL, true);

    if (firstController.position() != 6400L ||
        firstDriver.forwardPulses() != 6400UL ||
        firstDriver.backwardPulses() != 0UL ||
        secondController.position() != 0L) {
        return 2;
    }

    firstController.update(20000UL, true);

    if (firstController.position() != 0L ||
        firstDriver.forwardPulses() != 6400UL ||
        firstDriver.backwardPulses() != 6400UL ||
        secondController.position() != 0L) {
        return 3;
    }

    secondController.update(5000UL, true);

    if (secondController.position() != 3200L ||
        secondDriver.forwardPulses() != 3200UL ||
        secondDriver.backwardPulses() != 0UL ||
        firstController.position() != 0L) {
        return 4;
    }

    return 0;
}
