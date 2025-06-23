#pragma once

#include <Arduino.h>

/**
 * @brief Motor ESC control class for ESP32
 * 
 * This class provides an interface to control brushless motor ESCs using the ESP32's
 * LEDC peripheral for PWM generation.
 */
class MotorESC {
public:
    // Motor states
    enum MotorState {
        STATE_IDLE,
        STATE_RUNNING,
        STATE_ACCELERATING,
        STATE_DECELERATING
    };

    /**
     * @brief Initialize the motor ESC
     * 
     * @param pin GPIO pin connected to the ESC signal line
     * @param minPulseUs Minimum pulse width in microseconds (typically 1000)
     * @param maxPulseUs Maximum pulse width in microseconds (typically 2000)
     * @param freq PWM frequency in Hz (typically 50Hz for standard ESCs)
     * @param resolution PWM resolution in bits (8-16 bits)
     * @param channel LEDC channel to use (0-15)
     * @return true if initialization was successful, false otherwise
     */
    static bool begin(uint8_t pin = MOTOR_ESC_PIN, 
                     uint16_t minPulseUs = 1000,
                     uint16_t maxPulseUs = 2000,
                     uint16_t freq = 50,
                     uint8_t resolution = 16,
                     uint8_t channel = 1);
    
    /**
     * @brief Run the motor control loop
     * 
     * This function should be called regularly (in the main loop)
     * to handle acceleration and deceleration smoothly.
     * 
     * @return Current motor state
     */
    static MotorState run();
    
    /**
     * @brief Arm the ESC
     * 
     * Sends the minimum pulse width for a specified duration to arm the ESC.
     * This should be called before sending any throttle commands.
     * 
     * @param armTimeMs Time to keep the ESC at minimum throttle (in milliseconds)
     */
    static void arm(uint16_t armTimeMs = 2000);
    
    /**
     * @brief Set the throttle level
     * 
     * @param throttlePercent Throttle percentage (0.0 to 100.0)
     * @param smooth Whether to accelerate smoothly to the target throttle
     * @param accelTimeMs Time in milliseconds to accelerate to the target throttle
     */
    static void setThrottle(float throttlePercent, bool smooth = false, unsigned long accelTimeMs = 1000);
    
    /**
     * @brief Set the raw pulse width
     * 
     * @param pulseWidthUs Pulse width in microseconds
     */
    static void setPulseWidth(uint16_t pulseWidthUs);
    
    /**
     * @brief Stop the motor (set throttle to 0)
     * 
     * @param smooth Whether to decelerate smoothly to stop
     * @param decelTimeMs Time in milliseconds to decelerate to stop
     */
    static void stop(bool smooth = false, unsigned long decelTimeMs = 2000);
    
    /**
     * @brief Get current throttle level
     * 
     * @return Current throttle percentage (0.0 to 100.0)
     */
    static float getCurrentThrottle();
    
    /**
     * @brief Get current motor state
     * 
     * @return Current motor state
     */
    static MotorState getState();
    
    /**
     * @brief Calibrate the ESC
     * 
     * Many ESCs require calibration to set the minimum and maximum throttle range.
     * This function performs a standard calibration sequence.
     * 
     * WARNING: Remove propellers before calibration!
     * 
     * @param calibTimeMs Time for each step of the calibration process (in milliseconds)
     */
    static void calibrate(uint16_t calibTimeMs = 5000);

private:
    static uint8_t _pin;
    static uint8_t _channel;
    static uint16_t _minPulseUs;
    static uint16_t _maxPulseUs;
    static uint16_t _freq;
    static uint8_t _resolution;
    static uint32_t _maxDuty;
    static float _currentThrottle;
    
    // State management variables
    static MotorState _state;
    static unsigned long _stateStartTime;
    static float _targetThrottle;
    static float _startThrottle;
    static unsigned long _transitionDuration;
};

// Useful presets for common ESC protocols
#define ESC_STANDARD    50    // Standard 50Hz frequency (20ms period)
#define ESC_ONESHOT125  4000  // OneShot125 protocol (125-250us pulse, 4kHz)
#define ESC_ONESHOT42   8000  // OneShot42 protocol (42-84us pulse, 8kHz)
#define ESC_MULTISHOT   16000 // Multishot protocol (5-25us pulse, 16kHz)
#define ESC_DSHOT       0     // Digital protocol, not PWM-based
