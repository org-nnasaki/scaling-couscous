#pragma once

#include <cstdint>

#include "sensor_base.hpp"
#include "spi_interface.hpp"

namespace sensor {
namespace drivers {

/// ADXL345 3-axis accelerometer driver (SPI)
class Accelerometer : public ISensor {
public:
    // ── Register addresses ────────────────────────────────────────────────
    static constexpr uint8_t DEVID_REG       = 0x00U;
    static constexpr uint8_t DEVID           = 0xE5U;
    static constexpr uint8_t OFSX_REG        = 0x1EU;
    static constexpr uint8_t OFSY_REG        = 0x1FU;
    static constexpr uint8_t OFSZ_REG        = 0x20U;
    static constexpr uint8_t BW_RATE_REG     = 0x2CU;
    static constexpr uint8_t POWER_CTL_REG   = 0x2DU;
    static constexpr uint8_t INT_ENABLE_REG  = 0x2EU;
    static constexpr uint8_t INT_MAP_REG     = 0x2FU;
    static constexpr uint8_t INT_SOURCE_REG  = 0x30U;
    static constexpr uint8_t DATA_FORMAT_REG = 0x31U;
    static constexpr uint8_t DATA_REG        = 0x32U;
    static constexpr uint8_t FIFO_CTL_REG    = 0x38U;
    static constexpr uint8_t FIFO_STATUS_REG = 0x39U;

    // ── POWER_CTL bits ────────────────────────────────────────────────────
    static constexpr uint8_t MEASURE_BIT = 0x08U;
    static constexpr uint8_t SLEEP_BIT   = 0x04U;

    // ── DATA_FORMAT bits ──────────────────────────────────────────────────
    static constexpr uint8_t FULL_RES_BIT = 0x08U;

    // ── Enums ─────────────────────────────────────────────────────────────
    enum class Range : uint8_t {
        G2  = 0x00U,
        G4  = 0x01U,
        G8  = 0x02U,
        G16 = 0x03U,
    };

    enum class DataRate : uint8_t {
        Hz12_5 = 0x06U,
        Hz25   = 0x07U,
        Hz50   = 0x08U,
        Hz100  = 0x09U,
        Hz200  = 0x0AU,  // power-on default
        Hz400  = 0x0BU,
        Hz800  = 0x0CU,
        Hz1600 = 0x0DU,
        Hz3200 = 0x0EU,
    };

    // ── Construction ──────────────────────────────────────────────────────
    explicit Accelerometer(ISPI& bus,
                           Range range    = Range::G2,
                           DataRate rate  = DataRate::Hz100);
    ~Accelerometer() override = default;

    // ── ISensor interface ─────────────────────────────────────────────────
    Status init() override;
    Status reset() override;
    bool isConnected() override;
    uint8_t getDeviceId() override;
    const char* getName() override;

    // ── Sensor-specific API ───────────────────────────────────────────────
    /// Read X/Y/Z acceleration in g.
    Status readAcceleration(float& x, float& y, float& z);

    /// Change measurement range at runtime.
    Status setRange(Range range);

private:
    ISPI&    bus_;
    Range    range_;
    DataRate rate_;
    uint8_t  deviceId_{0U};
    bool     initialized_{false};

    float scaleFactor() const;
};

} // namespace drivers
} // namespace sensor
