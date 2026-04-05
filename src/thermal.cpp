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
static float _frameMax = 0.0f;static float _frameAmbient = 25.0f;  // ambient temperature from sensor (°C)
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

// Verify calibration EEPROM integrity
static void _verify_eeprom() {
    DEBUG_println(FST("  EEPROM Verification:"));
    
    // MLX90640 calibration stored in EEPROM at I2C address 0x33
    // Subpage 0 (pixel calib): 0x2400-0x24FF
    // Subpage 1 (other calib):  0x3400-0x34FF
    
    uint8_t eepromData[32] = {0};
    uint16_t startAddr = 0x2410;  // Sample from pixel calib region
    
    // Read 32 bytes from EEPROM starting at 0x2410
    bool readOk = true;
    for (uint8_t i = 0; i < 32; i++) {
        _wire1.beginTransmission(0x33);  // MLX90640 EEPROM address
        _wire1.write((startAddr + i) >> 8);   // High byte of address
        _wire1.write((startAddr + i) & 0xFF); // Low byte of address
        if (_wire1.endTransmission(false) != 0) {
            readOk = false;
            break;
        }
        
        _wire1.requestFrom(0x33, (uint8_t)1, (uint8_t)true);
        if (_wire1.available()) {
            eepromData[i] = _wire1.read();
        } else {
            readOk = false;
            break;
        }
    }
    
    if (!readOk) {
        DEBUG_println(FST("    ✗ Failed to read EEPROM"));
        return;
    }
    
    // Check for corruption patterns
    uint8_t allZeros = 1, allOnes = 1, allSame = 1;
    for (uint8_t i = 0; i < 32; i++) {
        if (eepromData[i] != 0x00) allZeros = 0;
        if (eepromData[i] != 0xFF) allOnes = 0;
        if (i > 0 && eepromData[i] != eepromData[0]) allSame = 0;
    }
    
    if (allZeros) {
        DEBUG_println(FST("    ✗ EEPROM appears uninitialized (all 0x00)"));
        DEBUG_println(FST("    → Sensor calibration data missing! Ta readings unreliable."));
        return;
    }
    
    if (allOnes) {
        DEBUG_println(FST("    ✗ EEPROM appears erased (all 0xFF)"));
        DEBUG_println(FST("    → Sensor calibration corrupted! Ta readings unreliable."));
        return;
    }
    
    if (allSame) {
        DEBUG_println(FST("    ⚠ WARNING: EEPROM data is uniform (possible corruption)"));
        return;
    }
    
    // EEPROM looks valid - show data variance
    uint8_t minVal = 255, maxVal = 0;
    for (uint8_t i = 0; i < 32; i++) {
        if (eepromData[i] < minVal) minVal = eepromData[i];
        if (eepromData[i] > maxVal) maxVal = eepromData[i];
    }
    
    // Check for full-range pattern (0x00-0xFF) which is suspicious
    if (minVal == 0x00 && maxVal == 0xFF) {
        DEBUG_printf(FST("    ⚠ WARNING: Full range 0x00-0xFF detected (possible I2C issue or corruption)\n"));
        DEBUG_println(FST("    → EEPROM reads may be invalid. Try I2C bus reset or sensor power cycle."));
        return;
    }
    
    DEBUG_printf(FST("    ✓ EEPROM data valid (range: 0x%02X-0x%02X)\n"), minVal, maxVal);
}

// Verify calibration data integrity
static void _verify_calibration() {
    DEBUG_println(FST("# MLX90640 Calibration Verification:"));
    
    // Verify EEPROM first
    _verify_eeprom();
    
    // Try to get a test frame with ambient temp to see raw values
    float testFrame[768] = {0};
    delay(100);  // Wait for sensor to have fresh data
    
    if (_mlx.getFrame(testFrame) == 0) {
        float ta = _mlx.getTa();
        float taMin = testFrame[0];
        float taMax = testFrame[0];
        
        // Check frame temperature stats
        for (int i = 0; i < 768; i++) {
            if (testFrame[i] < taMin) taMin = testFrame[i];
            if (testFrame[i] > taMax) taMax = testFrame[i];
        }
        
        DEBUG_printf(FST("  Frame Data:\n"));
        DEBUG_printf(FST("    Ta (Ambient): %.2f°C\n"), ta);
        DEBUG_printf(FST("    Frame Min: %.2f°C, Max: %.2f°C\n"), taMin, taMax);
        DEBUG_printf(FST("    Frame Range: %.2f°C\n"), (taMax - taMin));
        
        // Sanity checks
        if (ta < 10.0f || ta > 60.0f) {
            DEBUG_printf(FST("    ⚠ WARNING: Ambient %.2f°C seems unrealistic (expect 15-35°C)\n"), ta);
            DEBUG_println(FST("    → Check sensor mounting & thermal isolation"));
        }
        
        if ((taMax - taMin) > 0.5f) {
            DEBUG_println(FST("    ℹ Frame shows thermal gradient (motor/PCB heating sensor area)"));
        }
    } else {
        DEBUG_println(FST("  ✗ Failed to read frame during calibration check"));
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
    
    // Verify calibration after successful init
    _verify_calibration();
    
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

    // Extract ambient temperature from sensor
    _frameAmbient = _mlx.getTa();

    return true;
}

const float* thermal_get_frame()         { return _available ? _frame : nullptr; }
bool         thermal_is_available()      { return _available; }
unsigned long thermal_get_frame_age_ms() { return _available ? (millis() - _lastFrameMs) : UINT32_MAX; }
float        thermal_get_frame_max()     { return _frameMax; }
float        thermal_get_frame_ambient() { return _frameAmbient; }
