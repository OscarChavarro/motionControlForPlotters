#include "SteperMotorDriver.h"

const char*
steperMotorDriverName(SteperMotorDriver driver)
{
    switch (driver) {
        case STEPER_MOTOR_DRIVER_A4988:
            return "A4988";
        case STEPER_MOTOR_DRIVER_FS31W01_191202:
            return "FS31W01 191202";
        case STEPER_MOTOR_DRIVER_TMC2209:
            return "TMC2209";
    }
    return "unknown";
}
