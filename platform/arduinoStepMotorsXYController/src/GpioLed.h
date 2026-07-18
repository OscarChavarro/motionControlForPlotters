#ifndef XY_MOTION_CONTROLLER_GPIOLED_H
#define XY_MOTION_CONTROLLER_GPIOLED_H

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
