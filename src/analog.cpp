#include "analog.h"

/*=====================================================================*\
 | ESP32 measurable input voltage range:
 | -----------------+-----------------
 | ADC_ATTEN_DB_0   | 100 mV ~  950 mV
 | ADC_ATTEN_DB_2_5 | 100 mV ~ 1250 mV
 | ADC_ATTEN_DB_6   | 150 mV ~ 1750 mV
 | ADC_ATTEN_DB_11  | 150 mV ~ 2450 mV
 | -----------------+-----------------
 |
 | * 10K Pot: R1+R2 = 10K / 2.3 = 4.348K; R1 = 652; R2 = 3.696
 | * 20K Pot: R1+R2 = 20K / 2.3 = 8.696K; R1 = 1304; R2 = 7.392
 *=====================================================================*
 | ADC2 is used by WIFI and needs some extra logic to deal with conflict.
 | ADC2 GPIO: 0, 2, 4, 12, 13, 14, 15, 25, 26, 27
\*=====================================================================*/

void RolingAcc::reset() {
  for (size_t i=0; i<size_; i++) { buffer_[i] = 0.0; }
  sum_ = 0.0;
  fill_ = 0;
  n_ = 0;
}

float RolingAcc::avg(float v) {
  sum_ += v;
  sum_ -= buffer_[n_];
  buffer_[n_++] = v;
  if (n_ >= size_) { n_ = 0; }
  if (fill_ < size_) { fill_++; }
  return sum_ / fill_;
}


Potentiometer::Potentiometer(int8_t pin_, uint16_t minVal_, uint16_t maxVal_, float fc_) :
        pin(pin_), minVal(minVal_), maxVal(maxVal_), fc(fc_)  {
    fvalue = 0.0;
}

// Returns values between 0.0 and +1.0
float Potentiometer::read() {
    if (pin < 0) { return 0.0; }
    raw = analogRead(pin);
    value = (float) (raw - minVal) / float(maxVal - minVal);
    if (fc == 0.0) { return fvalue = value; }
    else if (fc > 0.0 && fc < 0.5) { fvalue = fvalue * (1.0 - fc) + value * fc; }
    else { fvalue = acc.avg((float) value); }
    return fvalue;
}
