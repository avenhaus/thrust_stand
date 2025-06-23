#include <Arduino.h>
#include "config.h"
#include "motor.h"
#include "driver/ledc.h"

// Initialize static member variables
uint8_t MotorESC::_pin = MOTOR_ESC_PIN;
uint8_t MotorESC::_channel = 1;
uint16_t MotorESC::_minPulseUs = 1000;
uint16_t MotorESC::_maxPulseUs = 2000;
uint16_t MotorESC::_freq = 50;
uint8_t MotorESC::_resolution = 16;
uint32_t MotorESC::_maxDuty = 0;
float MotorESC::_currentThrottle = 0.0f;

// Initialize state management variables
MotorESC::MotorState MotorESC::_state = MotorESC::STATE_IDLE;
unsigned long MotorESC::_stateStartTime = 0;
float MotorESC::_targetThrottle = 0.0f;
float MotorESC::_startThrottle = 0.0f;
unsigned long MotorESC::_transitionDuration = 1000;

bool MotorESC::begin(uint8_t pin, uint16_t minPulseUs, uint16_t maxPulseUs, 
                    uint16_t freq, uint8_t resolution, uint8_t channel) {
    // Store parameters
    _pin = pin;
    _minPulseUs = minPulseUs;
    _maxPulseUs = maxPulseUs;
    _freq = freq;
    _resolution = resolution;
    _channel = channel;
    _currentThrottle = 0.0f;
    _state = STATE_IDLE;
    
    // Calculate maximum duty cycle value based on resolution
    _maxDuty = (1 << resolution) - 1;
    
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .duty_resolution  = (ledc_timer_bit_t)resolution,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = freq,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    
    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
        DEBUG_println(FST("Failed to configure LEDC timer"));
        return false;
    }
    
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = pin,
        .speed_mode     = LEDC_HIGH_SPEED_MODE,
        .channel        = (ledc_channel_t)channel,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0, // Initially off
        .hpoint         = 0
    };
    
    if (ledc_channel_config(&ledc_channel) != ESP_OK) {
        DEBUG_println(FST("Failed to configure LEDC channel"));
        return false;
    }
    
    DEBUG_printf(FST("Motor ESC initialized on pin %d, freq: %dHz, resolution: %dbits\n"), 
                 pin, freq, resolution);
    DEBUG_printf(FST("Pulse width range: %d-%dus\n"), minPulseUs, maxPulseUs);
    
    // Set initial throttle to 0 (minimum pulse width)
    setThrottle(0);
    
    return true;
}

MotorESC::MotorState MotorESC::run() {
    // This function handles acceleration and deceleration
    if (_state == STATE_ACCELERATING || _state == STATE_DECELERATING) {
        unsigned long now = millis();
        unsigned long elapsed = now - _stateStartTime;
        
        if (elapsed >= _transitionDuration) {
            // Transition complete - set final throttle
            if (_state == STATE_ACCELERATING) {
                // Apply final throttle
                setThrottle(_targetThrottle, false);
                _state = STATE_RUNNING;
                // DEBUG_printf(FST("# Acceleration complete: %.2f%%\n"), _targetThrottle);
            } else { // STATE_DECELERATING
                // Final stop
                setThrottle(0, false);
                _state = STATE_IDLE;
                DEBUG_println(FST("# Deceleration complete, motor stopped."));
            }
        } else {
            // Calculate transition progress
            float progress = (float)elapsed / _transitionDuration;
            float throttle = 0.0f;
            
            if (_state == STATE_ACCELERATING) {
                // Linear acceleration
                throttle = _startThrottle + (_targetThrottle - _startThrottle) * progress;
                
                // Alternative: Smooth acceleration using easing function (smoother start and end)
                // float easeInOut = (1.0 - cos(progress * M_PI)) / 2.0;
                // float throttle = _startThrottle + (_targetThrottle - _startThrottle) * easeInOut                
            } else { // STATE_DECELERATING
                // Linear deceleration
                throttle = _startThrottle * (1.0 - progress);
                
                // Alternative: Exponential deceleration
                // float throttle = _startThrottle * exp(-5.0 * progress);                
            }

            setThrottle(throttle, false);
        }
    }
    
    return _state;
}

void MotorESC::arm(uint16_t armTimeMs) {
    DEBUG_println(FST("Arming ESC..."));
    
    // Send minimum throttle signal to arm the ESC
    setPulseWidth(_minPulseUs);
    
    // Wait for the specified time
    delay(armTimeMs);
    
    DEBUG_println(FST("ESC armed"));
    _state = STATE_IDLE;
}

void MotorESC::setThrottle(float throttlePercent, bool smooth, unsigned long accelTimeMs) {
    // Constrain input to valid range
    throttlePercent = constrain(throttlePercent, 0.0f, 100.0f);
    
    if (smooth && throttlePercent != _currentThrottle) {
        // Begin smooth acceleration to target throttle
        _startThrottle = _currentThrottle;
        _targetThrottle = throttlePercent;
        _transitionDuration = accelTimeMs;
        _stateStartTime = millis();
        _state = STATE_ACCELERATING;
        
        // DEBUG_printf(FST("# Starting acceleration from %.2f%% to %.2f%%\n"), _currentThrottle, _targetThrottle);
    } else {
        // Apply throttle immediately
        // Convert throttle percentage to pulse width
        uint16_t pulseWidth = map(throttlePercent * 100, 0, 10000, _minPulseUs, _maxPulseUs);
        
        // Apply the pulse width
        setPulseWidth(pulseWidth);
        
        // Store current throttle value
        _currentThrottle = throttlePercent;
        
        // Update state if needed
        if (_currentThrottle > 0 && _state == STATE_IDLE) {
            _state = STATE_RUNNING;
        } else if (_currentThrottle == 0 && _state == STATE_RUNNING) {
            _state = STATE_IDLE;
        }
    }
}

void MotorESC::setPulseWidth(uint16_t pulseWidthUs) {
    // Constrain pulse width to valid range
    pulseWidthUs = constrain(pulseWidthUs, _minPulseUs, _maxPulseUs);
    
    // Calculate duty cycle value based on pulse width using a more precise method
    // To avoid overflow, first calculate the ratio of desired pulse width to max period
    
    // First calculate the period in microseconds
    uint32_t periodUs = 1000000 / _freq;
    
    // Then calculate duty as a proportion of the period
    // Using floating point for intermediate calculation to avoid overflow
    float dutyCycleRatio = (float)pulseWidthUs / periodUs;
    uint32_t duty = (uint32_t)(dutyCycleRatio * _maxDuty);
    
    // Ensure duty is within valid range for the configured resolution
    duty = constrain(duty, 0, _maxDuty);
    
    // Apply duty cycle using ESP-IDF API
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)_channel, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)_channel);
    
    // Update stored throttle value
    _currentThrottle = map(pulseWidthUs, _minPulseUs, _maxPulseUs, 0, 10000) / 100.0f;
}

void MotorESC::stop(bool smooth, unsigned long decelTimeMs) {
    if (smooth && _currentThrottle > 0) {
        // Begin smooth deceleration to stop
        _startThrottle = _currentThrottle;
        _targetThrottle = 0;
        _transitionDuration = decelTimeMs;
        _stateStartTime = millis();
        _state = STATE_DECELERATING;
        
        DEBUG_printf(FST("# Starting smooth deceleration from %.2f%%\n"), _currentThrottle);
    } else {
        // Stop immediately
        setThrottle(0, false);
        _state = STATE_IDLE;
        DEBUG_println(FST("Motor stopped"));
    }
}

float MotorESC::getCurrentThrottle() {
    return _currentThrottle;
}

MotorESC::MotorState MotorESC::getState() {
    return _state;
}

void MotorESC::calibrate(uint16_t calibTimeMs) {
    DEBUG_println(FST("WARNING: ESC calibration started - REMOVE PROPELLERS!"));
    DEBUG_println(FST("Starting calibration sequence..."));
    
    // Step 1: Send maximum pulse width
    DEBUG_println(FST("Setting maximum throttle..."));
    setPulseWidth(_maxPulseUs);
    delay(calibTimeMs);
    
    // Step 2: Send minimum pulse width
    DEBUG_println(FST("Setting minimum throttle..."));
    setPulseWidth(_minPulseUs);
    delay(calibTimeMs);
    
    // Step 3: Complete calibration (usually signaled by beeps from ESC)
    DEBUG_println(FST("Calibration complete"));
    _state = STATE_IDLE;
}