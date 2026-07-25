#include "drivers/tmc/Tmc2209Registers.h"

Tmc2209GlobalConfig::Tmc2209GlobalConfig()
    : useVrefForCurrentScale(false),
      useInternalSenseResistors(false),
      enableSpreadCycle(false),
      inverseMotorDirection(false),
      indexShowsOvertemperaturePrewarning(false),
      indexShowsStepPulses(false),
      disableStandstillCurrentReductionPin(false),
      microStepResolutionFromRegister(false),
      filterStepPulses(false)
{
}

uint32_t
Tmc2209GlobalConfig::toRegisterValue() const
{
    uint32_t value = 0UL;
    if (useVrefForCurrentScale) {
        value |= (1UL << 0);
    }
    if (useInternalSenseResistors) {
        value |= (1UL << 1);
    }
    if (enableSpreadCycle) {
        value |= (1UL << 2);
    }
    if (inverseMotorDirection) {
        value |= (1UL << 3);
    }
    if (indexShowsOvertemperaturePrewarning) {
        value |= (1UL << 4);
    }
    if (indexShowsStepPulses) {
        value |= (1UL << 5);
    }
    if (disableStandstillCurrentReductionPin) {
        value |= (1UL << 6);
    }
    if (microStepResolutionFromRegister) {
        value |= (1UL << 7);
    }
    if (filterStepPulses) {
        value |= (1UL << 8);
    }
    return value;
}

Tmc2209GlobalConfig
Tmc2209GlobalConfig::fromRegisterValue(uint32_t value)
{
    Tmc2209GlobalConfig config;
    config.useVrefForCurrentScale = (value & (1UL << 0)) != 0UL;
    config.useInternalSenseResistors = (value & (1UL << 1)) != 0UL;
    config.enableSpreadCycle = (value & (1UL << 2)) != 0UL;
    config.inverseMotorDirection = (value & (1UL << 3)) != 0UL;
    config.indexShowsOvertemperaturePrewarning =
        (value & (1UL << 4)) != 0UL;
    config.indexShowsStepPulses = (value & (1UL << 5)) != 0UL;
    config.disableStandstillCurrentReductionPin =
        (value & (1UL << 6)) != 0UL;
    config.microStepResolutionFromRegister = (value & (1UL << 7)) != 0UL;
    config.filterStepPulses = (value & (1UL << 8)) != 0UL;
    return config;
}

Tmc2209CurrentControl::Tmc2209CurrentControl()
    : standstillCurrentScale(0U), runCurrentScale(0U), standstillDelay(0U)
{
}

Tmc2209CurrentControl::Tmc2209CurrentControl(
    uint8_t standstillCurrentScale,
    uint8_t runCurrentScale,
    uint8_t standstillDelay)
    : standstillCurrentScale(standstillCurrentScale),
      runCurrentScale(runCurrentScale),
      standstillDelay(standstillDelay)
{
}

uint32_t
Tmc2209CurrentControl::toRegisterValue() const
{
    uint32_t value = 0UL;
    value |= (static_cast<uint32_t>(standstillCurrentScale) & 0x1FUL);
    value |= (static_cast<uint32_t>(runCurrentScale) & 0x1FUL) << 8;
    value |= (static_cast<uint32_t>(standstillDelay) & 0x0FUL) << 16;
    return value;
}

Tmc2209ChopperConfig::Tmc2209ChopperConfig()
    : offTime(0U),
      blankTime(0U),
      interpolateToMicroStep256(false),
      microStepResolution(TMC2209_MICROSTEPS_256),
      lowSenseResistorVoltage(false)
{
}

uint32_t
Tmc2209ChopperConfig::toRegisterValue() const
{
    uint32_t value = 0UL;
    value |= (static_cast<uint32_t>(offTime) & 0x0FUL);
    value |= (static_cast<uint32_t>(blankTime) & 0x03UL) << 15;
    if (lowSenseResistorVoltage) {
        value |= (1UL << 17);
    }
    value |= (static_cast<uint32_t>(microStepResolution) & 0x0FUL) << 24;
    if (interpolateToMicroStep256) {
        value |= (1UL << 28);
    }
    return value;
}

Tmc2209ChopperConfig
Tmc2209ChopperConfig::fromRegisterValue(uint32_t value)
{
    Tmc2209ChopperConfig config;
    config.offTime = static_cast<uint8_t>(value & 0x0FUL);
    config.blankTime = static_cast<uint8_t>((value >> 15) & 0x03UL);
    config.lowSenseResistorVoltage = (value & (1UL << 17)) != 0UL;
    config.microStepResolution = static_cast<Tmc2209MicroStepResolution>(
        (value >> 24) & 0x0FUL);
    config.interpolateToMicroStep256 = (value & (1UL << 28)) != 0UL;
    return config;
}

Tmc2209DriverStatus::Tmc2209DriverStatus()
    : standstill(false),
      stealthChopActive(false),
      actualCurrentScale(0U),
      temperatureAbove157C(false),
      temperatureAbove150C(false),
      temperatureAbove143C(false),
      temperatureAbove120C(false),
      openLoadPhaseB(false),
      openLoadPhaseA(false),
      shortToSupplyPhaseB(false),
      shortToSupplyPhaseA(false),
      shortToGroundPhaseB(false),
      shortToGroundPhaseA(false),
      overtemperatureShutdown(false),
      overtemperaturePrewarning(false)
{
}

Tmc2209DriverStatus
Tmc2209DriverStatus::fromRegisterValue(uint32_t value)
{
    Tmc2209DriverStatus status;
    status.standstill = (value & (1UL << 31)) != 0UL;
    status.stealthChopActive = (value & (1UL << 30)) != 0UL;
    status.actualCurrentScale = static_cast<uint8_t>((value >> 16) & 0x1FUL);
    status.temperatureAbove157C = (value & (1UL << 11)) != 0UL;
    status.temperatureAbove150C = (value & (1UL << 10)) != 0UL;
    status.temperatureAbove143C = (value & (1UL << 9)) != 0UL;
    status.temperatureAbove120C = (value & (1UL << 8)) != 0UL;
    status.openLoadPhaseB = (value & (1UL << 7)) != 0UL;
    status.openLoadPhaseA = (value & (1UL << 6)) != 0UL;
    status.shortToSupplyPhaseB = (value & (1UL << 5)) != 0UL;
    status.shortToSupplyPhaseA = (value & (1UL << 4)) != 0UL;
    status.shortToGroundPhaseB = (value & (1UL << 3)) != 0UL;
    status.shortToGroundPhaseA = (value & (1UL << 2)) != 0UL;
    status.overtemperatureShutdown = (value & (1UL << 1)) != 0UL;
    status.overtemperaturePrewarning = (value & (1UL << 0)) != 0UL;
    return status;
}
