#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include "config.h"
#include "thermal.h"
#include <Adafruit_MLX90640.h>

// ---------------------------------------------------------------------------
//  Dedicated I2C bus for MLX90640
// ---------------------------------------------------------------------------
static TwoWire _wire1(1);   // ESP32 second hardware I2C controller

// ---------------------------------------------------------------------------
//  Private state
// ---------------------------------------------------------------------------
static Adafruit_MLX90640 _mlx;
static float           _frame[THERMAL_PIXELS];      // latest complete frame (°C)
static bool            _available   = false;
static unsigned long   _lastFrameMs = 0;
static float _frameMax = 0.0f;

// Timing for non-blocking reads
static unsigned long _lastReadAttemptMs = 0;
static const unsigned long _readIntervalMs = 100; // ~10 Hz attempt rate (sensor at 8 Hz)

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
static void _computeMaxTemps() {
    _frameMax = -999.0f;

    for (uint8_t r = 0; r < THERMAL_ROWS; r++) {
        for (uint8_t c = 0; c < THERMAL_COLS; c++) {
            float t = _frame[r * THERMAL_COLS + c];
            if (t > _frameMax) _frameMax = t;
        }
    }
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

bool thermal_init() {
    DEBUG_println(FST("# Initialize MLX90640 Thermal Sensor ..."));

    // Start dedicated I2C bus for the thermal sensor
    _wire1.begin(MLX90640_I2C_SDA_PIN, MLX90640_I2C_SCL_PIN, MLX90640_I2C_FREQ);

    if (!_mlx.begin(MLX90640_I2C_ADDR, &_wire1)) {
        DEBUG_println(FST("# MLX90640 not found on I2C1 bus (GPIO 16/17)."));
        _available = false;
        return false;
    }

    // Configure sensor
    _mlx.setMode(MLX90640_CHESS);
    _mlx.setResolution(MLX90640_ADC_19BIT);
    _mlx.setRefreshRate(MLX90640_8_HZ);

    // Initialise the frame buffer to ambient-ish temp
    for (int i = 0; i < THERMAL_PIXELS; i++) _frame[i] = 25.0f;

    _available = true;
    _lastFrameMs = millis();

    DEBUG_printf(FST("# MLX90640 serial: %04X %04X %04X\n"),
                 _mlx.serialNumber[0], _mlx.serialNumber[1], _mlx.serialNumber[2]);
    DEBUG_println(FST("# MLX90640 Thermal Sensor initialized."));
    return true;
}

bool thermal_update() {
    if (!_available) return false;

    // Rate-limit read attempts to avoid blocking the loop too often
    unsigned long now = millis();
    if (now - _lastReadAttemptMs < _readIntervalMs) return false;
    _lastReadAttemptMs = now;

    // getFrame() blocks while reading a full frame (~25-50ms at 8 Hz).
    if (_mlx.getFrame(_frame) != 0) {
        return false;  // read failed — keep previous frame
    }

    _lastFrameMs = millis();
    _computeMaxTemps();
    return true;
}

const float* thermal_get_frame()         { return _available ? _frame : nullptr; }
bool         thermal_is_available()      { return _available; }
unsigned long thermal_get_frame_age_ms() { return _available ? (millis() - _lastFrameMs) : UINT32_MAX; }
float        thermal_get_frame_max()     { return _frameMax; }
