#include "AvrMotorDriverEnable.h"

#include "AvrStepDirectionDriver.h"

AvrMotorDriverEnable::AvrMotorDriverEnable()
    : m_outputRegister(nullptr),
      m_bitMask(0)
{
}

void
AvrMotorDriverEnable::initialize()
{
    volatile uint8_t* const directionRegister =
        AvrStepDirectionDriver::directionRegisterForPin(
            MOTOR_DRIVER_ENABLE_PIN);
    m_outputRegister =
        AvrStepDirectionDriver::outputRegisterForPin(MOTOR_DRIVER_ENABLE_PIN);
    m_bitMask = AvrStepDirectionDriver::bitMaskForPin(MOTOR_DRIVER_ENABLE_PIN);

    if (directionRegister != nullptr && m_bitMask != 0U) {
        *directionRegister =
            static_cast<uint8_t>(*directionRegister | m_bitMask);
    }

    // Disabled (pin HIGH) until the firmware decides motors may run: the
    // shield's own pull-up already defaults here at reset, but this makes
    // the safe state explicit rather than relying on external hardware.
    setEnabled(false);
}

void
AvrMotorDriverEnable::setEnabled(bool enabled)
{
    if (m_outputRegister == nullptr || m_bitMask == 0U) {
        return;
    }

    // Active low: the shared driver enable line is asserted by pulling it
    // LOW, and released (drivers disabled) by driving it HIGH.
    if (enabled) {
        *m_outputRegister =
            static_cast<uint8_t>(*m_outputRegister & ~m_bitMask);
    }
    else {
        *m_outputRegister =
            static_cast<uint8_t>(*m_outputRegister | m_bitMask);
    }
}
