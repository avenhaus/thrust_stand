#pragma

#include <Arduino.h>

class RolingAcc {
public:
    RolingAcc() : size_(10) { reset(); }
    void reset();
    float avg(float v);

protected:
    float buffer_[10];
    float sum_;
    size_t size_;
    size_t fill_;
    size_t n_;
};

class Potentiometer {
public:
    Potentiometer(int8_t pin_,
        uint16_t minVal_ = 0,
        uint16_t maxVal_ = 4095,
        float fc_ = -1.0);

    float read();

    int8_t pin;
    uint16_t minVal;
    uint16_t maxVal;
    int16_t raw;
    float value;
    float fc;
    float fvalue;
    RolingAcc acc;
};
