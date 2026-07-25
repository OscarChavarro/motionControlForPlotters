#include "drivers/tmc/Tmc2209Registers.h"

int
main()
{
    // GCONF: pdn_disable + mstep_reg_select + multistep_filt = 0x1C0.
    Tmc2209GlobalConfig gconf;
    gconf.disableStandstillCurrentReductionPin = true;
    gconf.microStepResolutionFromRegister = true;
    gconf.filterStepPulses = true;
    if (gconf.toRegisterValue() != 0x000001C0UL) {
        return 1;
    }

    const Tmc2209GlobalConfig decodedGconf =
        Tmc2209GlobalConfig::fromRegisterValue(0x000001C0UL);
    if (!decodedGconf.disableStandstillCurrentReductionPin ||
        !decodedGconf.microStepResolutionFromRegister ||
        !decodedGconf.filterStepPulses ||
        decodedGconf.useVrefForCurrentScale ||
        decodedGconf.enableSpreadCycle) {
        return 2;
    }

    // IHOLD_IRUN: IHOLD=6, IRUN=13, IHOLDDELAY=4 => 0x00040D06.
    const Tmc2209CurrentControl current(6U, 13U, 4U);
    if (current.toRegisterValue() != 0x00040D06UL) {
        return 3;
    }

    // CHOPCONF: native 1/256 microsteps, interpolation on, TOFF=3, TBL=1.
    Tmc2209ChopperConfig chopper;
    chopper.offTime = 3U;
    chopper.blankTime = 1U;
    chopper.interpolateToMicroStep256 = true;
    chopper.microStepResolution = TMC2209_MICROSTEPS_256;
    const uint32_t chopperValue = chopper.toRegisterValue();

    const Tmc2209ChopperConfig decodedChopper =
        Tmc2209ChopperConfig::fromRegisterValue(chopperValue);
    if (decodedChopper.offTime != 3U || decodedChopper.blankTime != 1U ||
        !decodedChopper.interpolateToMicroStep256 ||
        decodedChopper.microStepResolution != TMC2209_MICROSTEPS_256 ||
        decodedChopper.lowSenseResistorVoltage) {
        return 4;
    }

    // DRV_STATUS: standstill + stealthChop active + otpw flag set.
    const uint32_t statusValue = (1UL << 31) | (1UL << 30) | (1UL << 0);
    const Tmc2209DriverStatus status =
        Tmc2209DriverStatus::fromRegisterValue(statusValue);
    if (!status.standstill || !status.stealthChopActive ||
        !status.overtemperaturePrewarning || status.overtemperatureShutdown) {
        return 5;
    }

    return 0;
}
