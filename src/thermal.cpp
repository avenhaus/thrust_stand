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
static bool            _roiDefault  = true;
static ThermalROI      _roi = {
    THERMAL_ROI_X_DEFAULT,
    THERMAL_ROI_Y_DEFAULT,
    THERMAL_ROI_W_DEFAULT,
    THERMAL_ROI_H_DEFAULT
};

static float _roiMax   = 0.0f;
static float _frameMax = 0.0f;

static Preferences _prefs;

// Timing for non-blocking reads
static unsigned long _lastReadAttemptMs = 0;
static const unsigned long _readIntervalMs = 100; // ~10 Hz attempt rate (sensor at 8 Hz)

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
static void _computeMaxTemps() {
    _frameMax = -999.0f;
    _roiMax   = -999.0f;

    for (uint8_t r = 0; r < THERMAL_ROWS; r++) {
        for (uint8_t c = 0; c < THERMAL_COLS; c++) {
            float t = _frame[r * THERMAL_COLS + c];
            if (t > _frameMax) _frameMax = t;

            if (c >= _roi.x && c < _roi.x + _roi.w &&
                r >= _roi.y && r < _roi.y + _roi.h) {
                if (t > _roiMax) _roiMax = t;
            }
        }
    }
    // If ROI was empty (shouldn't happen), fall back to frame max
    if (_roiMax < -900.0f) _roiMax = _frameMax;
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

    // Load saved ROI from NVS
    thermal_load_roi_from_nvs();

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
float        thermal_get_roi_max()       { return _roiMax; }
float        thermal_get_frame_max()     { return _frameMax; }

ThermalROI thermal_get_roi() { return _roi; }

bool thermal_set_roi(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    if (w == 0 || h == 0) return false;
    if (x + w > THERMAL_COLS || y + h > THERMAL_ROWS) return false;

    _roi = { x, y, w, h };
    _roiDefault = false;
    _computeMaxTemps();
    thermal_save_roi_to_nvs();
    return true;
}

bool thermal_roi_is_default() { return _roiDefault; }

void thermal_load_roi_from_nvs() {
    _prefs.begin("thrust_stand", true);  // read-only
    if (_prefs.isKey("roi_x")) {
        _roi.x = _prefs.getUChar("roi_x", THERMAL_ROI_X_DEFAULT);
        _roi.y = _prefs.getUChar("roi_y", THERMAL_ROI_Y_DEFAULT);
        _roi.w = _prefs.getUChar("roi_w", THERMAL_ROI_W_DEFAULT);
        _roi.h = _prefs.getUChar("roi_h", THERMAL_ROI_H_DEFAULT);
        // Validate loaded values
        if (_roi.x + _roi.w > THERMAL_COLS || _roi.y + _roi.h > THERMAL_ROWS ||
            _roi.w == 0 || _roi.h == 0) {
            _roi = { THERMAL_ROI_X_DEFAULT, THERMAL_ROI_Y_DEFAULT,
                     THERMAL_ROI_W_DEFAULT, THERMAL_ROI_H_DEFAULT };
            _roiDefault = true;
        } else {
            _roiDefault = false;
        }
    }
    _prefs.end();
    DEBUG_printf(FST("# Thermal ROI: x=%d y=%d w=%d h=%d %s\n"),
                 _roi.x, _roi.y, _roi.w, _roi.h,
                 _roiDefault ? "(default)" : "(saved)");
}

void thermal_save_roi_to_nvs() {
    _prefs.begin("thrust_stand", false);  // read-write
    _prefs.putUChar("roi_x", _roi.x);
    _prefs.putUChar("roi_y", _roi.y);
    _prefs.putUChar("roi_w", _roi.w);
    _prefs.putUChar("roi_h", _roi.h);
    _prefs.end();
}
