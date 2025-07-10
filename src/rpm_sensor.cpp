#include <Arduino.h>
#include "config.h"
#include "rpm_sensor.h"

// Initialize static member variables
volatile uint32_t RpmSensor::_pulseCount = 0;
volatile uint32_t RpmSensor::_lastPulseTime = 0;
volatile uint32_t RpmSensor::_pulseInterval = 0;

uint8_t RpmSensor::_pin = RPM_SENSOR_PIN;
uint8_t RpmSensor::_pulsesPerRevolution = 1;
float RpmSensor::_rpm = 0;
float RpmSensor::_rpmFiltered = 0;
uint32_t RpmSensor::_lastUpdateTime = 0;

float RpmSensor::_rpmHistory[RPM_AVERAGING] = {0};
uint8_t RpmSensor::_historyIndex = 0;

// Global variables accessible from main code
float rpm = 0;          // Current RPM value

bool RpmSensor::begin(uint8_t pin, uint8_t pulsesPerRevolution) {
    _pin = pin;
    _pulsesPerRevolution = pulsesPerRevolution > 0 ? pulsesPerRevolution : 1;
    
    // Reset variables
    _pulseCount = 0;
    _lastPulseTime = 0;
    _pulseInterval = 0;
    _rpm = 0;
    _rpmFiltered = 0;
    _lastUpdateTime = millis();
    
    // Reset history array
    for (uint8_t i = 0; i < RPM_AVERAGING; i++) {
        _rpmHistory[i] = 0;
    }
    _historyIndex = 0;
    
    // Configure GPIO pin
    pinMode(_pin, INPUT_PULLUP);  // Use internal pull-up resistor
    
    // Attach interrupt - FALLING edge detection for optical sensor with reflective surface
    // This assumes the sensor outputs LOW when light is reflected back
    attachInterrupt(digitalPinToInterrupt(_pin), pulseCounter, FALLING);
    
    DEBUG_printf(FST("RPM Sensor initialized on pin %d, %d pulse(s) per revolution\n"), 
                 pin, pulsesPerRevolution);
                 
    return true;
}

bool RpmSensor::update() {
    // Get the current time
    uint32_t now = millis();
    
    // Check if there have been any pulses recently
    if (now - (uint32_t)(_lastPulseTime / 1000) > RPM_TIMEOUT_MS) {
        // No pulses for a while, RPM is 0
        _rpm = 0;
    } else if (_pulseInterval > 0) {
        // Calculate RPM based on the interval between pulses
        // RPM = (60 seconds * 1,000,000 microseconds) / (pulse interval in microseconds * pulses per rev)
        _rpm = (60.0f * 1000000.0f) / (_pulseInterval * _pulsesPerRevolution);
    }
    
    // Add to history for filtering
    _rpmHistory[_historyIndex] = _rpm;
    _historyIndex = (_historyIndex + 1) % RPM_AVERAGING;
    
    // Calculate average RPM for smoother readings
    float sum = 0;
    for (uint8_t i = 0; i < RPM_AVERAGING; i++) {
        sum += _rpmHistory[i];
    }
    _rpmFiltered = sum / RPM_AVERAGING;
    
    // Update global variables
    rpm = _rpmFiltered;
    
    // Check if a new reading is available (we've received a pulse since last update)
    bool newReading = (_pulseCount > 0);
    
    // Update the last update time
    _lastUpdateTime = now;
    
    return newReading;
}

float RpmSensor::getRPM() {
    return _rpmFiltered;
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
    if (_lastPulseTime > 0) {
        uint32_t interval = now - _lastPulseTime;
        
        // Simple debounce - ignore pulses that come too quickly
        if (interval > (RPM_DEBOUNCE_MS * 1000)) {
            _pulseInterval = interval;
            _pulseCount++;
        }
    }
    
    // Update the last pulse time
    _lastPulseTime = now;
}