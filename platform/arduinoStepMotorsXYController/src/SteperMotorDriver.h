#ifndef STEPER_MOTOR_DRIVER_H
#define STEPER_MOTOR_DRIVER_H

enum SteperMotorDriver {
    STEPER_MOTOR_DRIVER_A4988,
    STEPER_MOTOR_DRIVER_FS31W01_191202,
    STEPER_MOTOR_DRIVER_TMC2209
};

const char*
steperMotorDriverName(SteperMotorDriver driver);

#endif
