#include <Arduino.h>
#include "config.h"
#include "rpm_sensor.h"

// Initialize static member variables
volatile uint32_t RpmSensor::_pulseCount = 0;
volatile uint32_t RpmSensor::_lastPulseTime = 0;
volatile uint32_t RpmSensor::_pulseInterval = 0;

// Initialize circular buffer for periods
volatile uint32_t RpmSensor::_periodBuffer[RPM_PERIOD_BUFFER_SIZE] = {0};
volatile uint8_t RpmSensor::_periodBufferIndex = 0;
uint32_t RpmSensor::_thresholdPeriod = 0; // Threshold for outlier detection

volatile uint32_t RpmSensor::_averageBuffer[RPM_AVERAGE_BUFFER_SIZE] = {0};
volatile uint8_t RpmSensor::_averageBufferIndex = 0;


float RpmSensor::_rpm = 0;          // Current RPM value
uint8_t RpmSensor::_pin = RPM_SENSOR_PIN;
uint8_t RpmSensor::_pulsesPerRevolution = 1;
uint32_t RpmSensor::_lastUpdateTime = 0;


bool RpmSensor::begin(uint8_t pin, uint8_t pulsesPerRevolution) {
    _pin = pin;
    _pulsesPerRevolution = pulsesPerRevolution > 0 ? pulsesPerRevolution : 1;
    
    // Reset variables
    _pulseCount = 0;
    _lastPulseTime = 0;
    _pulseInterval = 0;
    _rpm = 0;
    _thresholdPeriod = 0;
    _lastUpdateTime = millis();
    
    // Reset period buffer
    for (uint8_t i = 0; i < RPM_PERIOD_BUFFER_SIZE; i++) {
        _periodBuffer[i] = 0;
    }
    _periodBufferIndex = 0;
    
    // Reset average buffer
    for (uint8_t i = 0; i < RPM_AVERAGE_BUFFER_SIZE; i++) {
        _averageBuffer[i] = 0;
    }   
    
    // Configure GPIO pin
    pinMode(_pin, INPUT_PULLUP);  // Use internal pull-up resistor
    
    // Attach interrupt - FALLING edge detection for optical sensor with reflective surface
    // This assumes the sensor outputs LOW when light is reflected back
    attachInterrupt(digitalPinToInterrupt(_pin), pulseCounter, FALLING);
    
    DEBUG_printf(FST("RPM Sensor initialized on pin %d, %d pulse(s) per revolution\n"), 
                 pin, pulsesPerRevolution);
                 
    return true;
}

float RpmSensor::update() {
    // Get the current time
    uint32_t now = millis();
    
    // Check if there have been any pulses recently
    if (now - (uint32_t)(_lastPulseTime / 1000) > RPM_TIMEOUT_MS) {
        // No pulses for a while, RPM is 0
        _rpm = 0;
        _thresholdPeriod = 0;
        //Clear average and period buffers
        for (uint8_t i = 0; i < RPM_PERIOD_BUFFER_SIZE; i++) {
            _periodBuffer[i] = 0;
        }
        for (uint8_t i = 0; i < RPM_AVERAGE_BUFFER_SIZE; i++) {
            _averageBuffer[i] = 0;
        }
    } else if (_pulseInterval > 0) {

        // Calculate the average interval from the buffer, ignoring min and max values
        uint32_t minInterval = UINT32_MAX;
        uint32_t maxInterval = 0;
        float intervalSum = 0;

        // Find min and max values while counting valid entries
        for (uint8_t i = 0; i < RPM_AVERAGE_BUFFER_SIZE; i++) {
            if (_averageBuffer[i] > 0) {
                if (_averageBuffer[i] < minInterval) minInterval = _averageBuffer[i];
                if (_averageBuffer[i] > maxInterval) maxInterval = _averageBuffer[i];
                intervalSum += _averageBuffer[i];
            }
        }
        // Subtract min and max from the sum
        intervalSum -= (minInterval + maxInterval);
        // Calculate average excluding min and max
        float averageInterval = intervalSum / (RPM_AVERAGE_BUFFER_SIZE - 2);

        _rpm = (60.0f * 1000000.0f) / (averageInterval * _pulsesPerRevolution);
        
        // Find the maximum period
        uint32_t maxPeriod = 0;
        for (uint8_t i = 0; i < RPM_PERIOD_BUFFER_SIZE; i++) {
            if (_periodBuffer[i] > maxPeriod) {
                maxPeriod = _periodBuffer[i];
            }
        }
        _thresholdPeriod = maxPeriod * RPM_OUTLIER_THRESHOLD;
        //DEBUG_printf(FST(" Max period: %d us, threshold: %d us, pulseCount: %d %d  | rpm: %.0f |"), maxPeriod, _thresholdPeriod, _pulseCount, _pulseInterval, _rpm);
        
        //interrupts();
    }
    
    // Update the last update time
    _lastUpdateTime = now;
    
    return _rpm;
}

float RpmSensor::getRPM() {
    return _rpm;
}

uint32_t RpmSensor::getPulseCount(bool resetCounter) {
    // Read the pulse count atomically
    noInterrupts();
    uint32_t count = _pulseCount;
    if (resetCounter) {
        _pulseCount = 0;
    }
    interrupts();
    
    return count;
}



void IRAM_ATTR RpmSensor::pulseCounter() {
    // Get the current time in microseconds
    uint32_t now = micros();
    
    // Calculate the time since the last pulse
    if (_lastPulseTime == 0) {
        _lastPulseTime = now;
        return;
    }

    uint32_t interval = now - _lastPulseTime;
        
    // Simple debounce - ignore pulses that are way too quick
    if (interval <= (RPM_DEBOUNCE_US)) { return; }
    
    // Add to circular buffer
    _periodBuffer[_periodBufferIndex] = interval;
    _periodBufferIndex = (_periodBufferIndex + 1) % RPM_PERIOD_BUFFER_SIZE;
        
    if (interval < _thresholdPeriod) { return; } // Ignore outliers below the period threshold

    // Process the new interval
    _averageBuffer[_averageBufferIndex] = interval;
    _averageBufferIndex = (_averageBufferIndex + 1) % RPM_AVERAGE_BUFFER_SIZE;

    _pulseInterval = interval;
    _pulseCount++;
    _lastPulseTime = now;
}