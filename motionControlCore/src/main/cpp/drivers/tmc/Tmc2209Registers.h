#ifndef MOTION_CONTROL_DRIVERS_TMC_TMC2209_REGISTERS_H
#define MOTION_CONTROL_DRIVERS_TMC_TMC2209_REGISTERS_H

#include <stdint.h>

// Register addresses, chapter 5 "Register Map" of the TMC2209 datasheet.
enum Tmc2209RegisterAddress : uint8_t {
    TMC2209_REG_GCONF = 0x00,
    TMC2209_REG_GSTAT = 0x01,
    TMC2209_REG_IFCNT = 0x02,
    TMC2209_REG_NODECONF = 0x03,
    TMC2209_REG_OTP_PROG = 0x04,
    TMC2209_REG_OTP_READ = 0x05,
    TMC2209_REG_IOIN = 0x06,
    TMC2209_REG_FACTORY_CONF = 0x07,
    TMC2209_REG_IHOLD_IRUN = 0x10,
    TMC2209_REG_TPOWERDOWN = 0x11,
    TMC2209_REG_TSTEP = 0x12,
    TMC2209_REG_TPWMTHRS = 0x13,
    TMC2209_REG_TCOOLTHRS = 0x14,
    TMC2209_REG_VACTUAL = 0x22,
    TMC2209_REG_SGTHRS = 0x40,
    TMC2209_REG_SG_RESULT = 0x41,
    TMC2209_REG_COOLCONF = 0x42,
    TMC2209_REG_MSCNT = 0x6A,
    TMC2209_REG_MSCURACT = 0x6B,
    TMC2209_REG_CHOPCONF = 0x6C,
    TMC2209_REG_DRV_STATUS = 0x6F,
    TMC2209_REG_PWMCONF = 0x70,
    TMC2209_REG_PWM_SCALE = 0x71,
    TMC2209_REG_PWM_AUTO = 0x72
};

// CHOPCONF.MRES, section 5.5.1. Number of microsteps per step pulse is
// 2^(8-value) for values 0..8 (0 = native 256 microsteps, 8 = fullstep).
enum Tmc2209MicroStepResolution : uint8_t {
    TMC2209_MICROSTEPS_256 = 0U,
    TMC2209_MICROSTEPS_128 = 1U,
    TMC2209_MICROSTEPS_64 = 2U,
    TMC2209_MICROSTEPS_32 = 3U,
    TMC2209_MICROSTEPS_16 = 4U,
    TMC2209_MICROSTEPS_8 = 5U,
    TMC2209_MICROSTEPS_4 = 6U,
    TMC2209_MICROSTEPS_2 = 7U,
    TMC2209_MICROSTEPS_FULLSTEP = 8U
};

// GCONF (0x00), section 5.1. Global configuration flags.
struct Tmc2209GlobalConfig {
    bool useVrefForCurrentScale;
    bool useInternalSenseResistors;
    bool enableSpreadCycle;
    bool inverseMotorDirection;
    bool indexShowsOvertemperaturePrewarning;
    bool indexShowsStepPulses;
    bool disableStandstillCurrentReductionPin;
    bool microStepResolutionFromRegister;
    bool filterStepPulses;

    Tmc2209GlobalConfig();

    uint32_t toRegisterValue() const;
    static Tmc2209GlobalConfig fromRegisterValue(uint32_t value);
};

// IHOLD_IRUN (0x10), section 5.2. Standstill and run current control.
struct Tmc2209CurrentControl {
    uint8_t standstillCurrentScale; // IHOLD, 0..31 (0=1/32 ... 31=32/32)
    uint8_t runCurrentScale;        // IRUN, 0..31 (0=1/32 ... 31=32/32)
    uint8_t standstillDelay;        // IHOLDDELAY, 0..15

    Tmc2209CurrentControl();
    Tmc2209CurrentControl(
        uint8_t standstillCurrentScale,
        uint8_t runCurrentScale,
        uint8_t standstillDelay);

    uint32_t toRegisterValue() const;
};

// CHOPCONF (0x6C), section 5.5.1. Only the fields needed to select the
// microstep resolution and enable the driver are modeled; the SpreadCycle
// hysteresis fields (HSTRT/HEND) are intentionally left out, since they are
// not required when relying on StealthChop automatic tuning.
struct Tmc2209ChopperConfig {
    uint8_t offTime;      // TOFF, 0..15 (0 disables all bridges)
    uint8_t blankTime;    // TBL, 0..3
    bool interpolateToMicroStep256; // intpol
    Tmc2209MicroStepResolution microStepResolution; // MRES
    bool lowSenseResistorVoltage; // vsense

    Tmc2209ChopperConfig();

    uint32_t toRegisterValue() const;
    static Tmc2209ChopperConfig fromRegisterValue(uint32_t value);
};

// DRV_STATUS (0x6F), section 5.5.3. Read-only driver status flags.
struct Tmc2209DriverStatus {
    bool standstill;
    bool stealthChopActive;
    uint8_t actualCurrentScale;
    bool temperatureAbove157C;
    bool temperatureAbove150C;
    bool temperatureAbove143C;
    bool temperatureAbove120C;
    bool openLoadPhaseB;
    bool openLoadPhaseA;
    bool shortToSupplyPhaseB;
    bool shortToSupplyPhaseA;
    bool shortToGroundPhaseB;
    bool shortToGroundPhaseA;
    bool overtemperatureShutdown;
    bool overtemperaturePrewarning;

    Tmc2209DriverStatus();

    static Tmc2209DriverStatus fromRegisterValue(uint32_t value);
};

#endif
