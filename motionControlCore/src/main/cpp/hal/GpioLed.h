#ifndef MOTION_CONTROL_HAL_GPIO_LED_H
#define MOTION_CONTROL_HAL_GPIO_LED_H

class GpioLed {
  protected:
    ~GpioLed() = default;

  public:
    virtual void initialize() = 0;
    virtual void write(bool enabled) = 0;
};

#endif
