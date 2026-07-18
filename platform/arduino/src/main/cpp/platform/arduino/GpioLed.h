#ifndef MOTION_CONTROL_PLATFORM_ARDUINO_GPIOLED_H
#define MOTION_CONTROL_PLATFORM_ARDUINO_GPIOLED_H

class GpioLed {
public:
    void initialize();
    void write(bool enabled);

private:
    static volatile unsigned char* ledPort();
    static volatile unsigned char* ledDdr();
    static unsigned char ledBitMask();
};

#endif
