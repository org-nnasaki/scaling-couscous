#pragma once

#include <cstdint>

#include "i2c_interface.hpp"
#include "sensor_base.hpp"

namespace sensor {
namespace drivers {

/// BME280 temperature / pressure / humidity sensor driver (I2C)
class TemperatureSensor : public ISensor {
public:
    // ── Register addresses ────────────────────────────────────────────────
    static constexpr uint8_t DEFAULT_I2C_ADDR = 0x76U;
    static constexpr uint8_t CHIP_ID_REG      = 0xD0U;
    static constexpr uint8_t CHIP_ID          = 0x60U;
    static constexpr uint8_t RESET_REG        = 0xE0U;
    static constexpr uint8_t RESET_VALUE      = 0xB6U;
    static constexpr uint8_t CTRL_HUM_REG     = 0xF2U;
    static constexpr uint8_t STATUS_REG       = 0xF3U;
    static constexpr uint8_t CTRL_MEAS_REG    = 0xF4U;
    static constexpr uint8_t CONFIG_REG       = 0xF5U;
    static constexpr uint8_t DATA_REG         = 0xF7U;
    static constexpr uint8_t CALIB_TP_REG     = 0x88U;
    static constexpr uint8_t CALIB_H2_REG     = 0xE1U;

    // ── Calibration data ──────────────────────────────────────────────────
    struct CalibrationData {
        uint16_t dig_T1;
        int16_t  dig_T2;
        int16_t  dig_T3;
        uint16_t dig_P1;
        int16_t  dig_P2;
        int16_t  dig_P3;
        int16_t  dig_P4;
        int16_t  dig_P5;
        int16_t  dig_P6;
        int16_t  dig_P7;
        int16_t  dig_P8;
        int16_t  dig_P9;
        uint8_t  dig_H1;
        int16_t  dig_H2;
        uint8_t  dig_H3;
        int16_t  dig_H4;
        int16_t  dig_H5;
        int8_t   dig_H6;
    };

    // ── Construction ──────────────────────────────────────────────────────
    explicit TemperatureSensor(II2C& bus,
                               uint8_t addr = DEFAULT_I2C_ADDR);
    ~TemperatureSensor() override = default;

    // ── ISensor interface ─────────────────────────────────────────────────
    Status init() override;
    Status reset() override;
    bool isConnected() override;
    uint8_t getDeviceId() override;
    const char* getName() override;

    // ── Sensor-specific API ───────────────────────────────────────────────
    /// Read temperature in °C.
    Status readTemperature(float& temperature);

    /// Read pressure in Pa.
    Status readPressure(float& pressure);

    /// Read relative humidity in %RH.
    Status readHumidity(float& humidity);

    /// Read all three measurements in one burst.
    Status readAll(float& temperature, float& pressure, float& humidity);

    const CalibrationData& calibration() const { return calib_; }

private:
    II2C&           bus_;
    uint8_t         addr_;
    uint8_t         deviceId_{0U};
    bool            initialized_{false};
    CalibrationData calib_{};
    int32_t         t_fine_{0};

    Status  readCalibration();
    int32_t compensateTemperature(int32_t adcT);
    uint32_t compensatePressure(int32_t adcP);
    uint32_t compensateHumidity(int32_t adcH);
};

} // namespace drivers
} // namespace sensor
