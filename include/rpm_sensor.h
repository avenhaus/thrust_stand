#pragma once

#include <Arduino.h>

// Configuration for RPM sensor
#define RPM_SENSOR_PIN 13     // GPIO pin connected to the optical sensor (default: 13)
#define RPM_DEBOUNCE_MS 5     // Debounce time in milliseconds
#define RPM_TIMEOUT_MS 2000   // Maximum time in ms between pulses before RPM is considered 0
#define RPM_AVERAGING 10      // Number of measurements to average for stable readings

/**
 * @class RpmSensor
 * @brief Interrupt-driven RPM sensor for measuring motor/propeller rotation speed
 * 
 * Uses an optical sensor with laser reflection to count revolutions and calculate RPM
 */
class RpmSensor {
public:
    /**
     * @brief Initialize the RPM sensor
     * 
     * @param pin GPIO pin connected to the optical sensor
     * @param pulsesPerRevolution Number of pulses per complete revolution
     * @return true if initialization was successful
     */
    static bool begin(uint8_t pin = RPM_SENSOR_PIN, uint8_t pulsesPerRevolution = 1);
    
    /**
     * @brief Update RPM calculations - should be called regularly in the main loop
     * 
     * @return true if a new RPM value is available
     */
    static bool update();
    
    /**
     * @brief Get the current RPM value
     * 
     * @return Current RPM value
     */
    static float getRPM();
    
    /**
     * @brief Get raw pulse count since last call
     * 
     * @param resetCounter Whether to reset the counter after reading
     * @return Number of pulses detected
     */
    static uint32_t getPulseCount(bool resetCounter = true);

private:
    /**
     * @brief Interrupt service routine for RPM sensing
     */
    static void IRAM_ATTR pulseCounter();
    
    static volatile uint32_t _pulseCount;     // Number of pulses counted
    static volatile uint32_t _lastPulseTime;  // Timestamp of last pulse (in microseconds)
    static volatile uint32_t _pulseInterval;  // Interval between last two pulses (in microseconds)
    
    static uint8_t _pin;                      // GPIO pin for RPM sensor
    static uint8_t _pulsesPerRevolution;      // Number of pulses per revolution
    static float _rpm;                        // Calculated RPM value
    static float _rpmFiltered;                // Filtered/averaged RPM value
    static uint32_t _lastUpdateTime;          // Last time the RPM was updated
    
    // For averaging/filtering
    static float _rpmHistory[RPM_AVERAGING];  // Array to store RPM history
    static uint8_t _historyIndex;             // Current index in the history array
};

// Declare external variables for main code to access
extern float rpm;          // Current RPM value