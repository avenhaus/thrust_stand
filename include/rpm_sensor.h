#pragma once

#include <Arduino.h>

// Configuration for RPM sensor
#define RPM_DEBOUNCE_US 500    // Debounce time in microseconds
#define RPM_TIMEOUT_MS 2000   // Maximum time in ms between pulses before RPM is considered 0
#define RPM_PERIOD_BUFFER_SIZE 8  // Size of circular buffer for all (dirty) period measurements
#define RPM_OUTLIER_THRESHOLD 0.2  // Threshold for rejecting short outliers (50% of max period)
#define RPM_AVERAGE_BUFFER_SIZE 32      // Number of measurements to average for stable readings

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
     * @return the current RPM value
     */
    static float update();
    
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
    
    /**
     * @brief Validates and processes a new pulse interval
     * 
     * @param interval New pulse interval in microseconds
     * @return true if the interval is valid, false if it's an outlier
     */
    static bool IRAM_ATTR processNewInterval(uint32_t interval);
    
    static volatile uint32_t _pulseCount;     // Number of pulses counted
    static volatile uint32_t _lastPulseTime;  // Timestamp of last pulse (in microseconds)
    static volatile uint32_t _pulseInterval;  // Interval between last two pulses (in microseconds)
    
    // Circular buffer for period measurements
    static volatile uint32_t _periodBuffer[RPM_PERIOD_BUFFER_SIZE];  // Buffer of all recent periods
    static volatile uint8_t _periodBufferIndex;                      // Current index in buffer
    static uint32_t _thresholdPeriod;                               // Threshold for outlier detection (max period
    
    static volatile uint32_t _averageBuffer[RPM_AVERAGE_BUFFER_SIZE];  // Buffer of clean recent periods
    static volatile uint8_t _averageBufferIndex;                      // Current index in buffer

    static uint8_t _pin;                      // GPIO pin for RPM sensor
    static uint8_t _pulsesPerRevolution;      // Number of pulses per revolution
    static float _rpm;                        // Calculated RPM value
    static uint32_t _lastUpdateTime;          // Last time the RPM was updated    
};
