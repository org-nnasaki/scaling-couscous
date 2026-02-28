#include "drivers/accelerometer.hpp"

#include <cstdint>

namespace sensor {
namespace drivers {

Accelerometer::Accelerometer(ISPI& bus, Range range, DataRate rate)
    : bus_(bus), range_(range), rate_(rate) {}

// ── ISensor ───────────────────────────────────────────────────────────────

Status Accelerometer::init() {
    // 1. Verify device ID
    uint8_t id = 0U;
    Status st = bus_.readReg(DEVID_REG, &id, 1U);
    if (st != Status::Ok) { return st; }
    if (id != DEVID) { return Status::NotFound; }
    deviceId_ = id;

    // 2. DATA_FORMAT: FULL_RES mode + range
    uint8_t fmt = static_cast<uint8_t>(FULL_RES_BIT |
                                       static_cast<uint8_t>(range_));
    st = bus_.writeReg(DATA_FORMAT_REG, &fmt, 1U);
    if (st != Status::Ok) { return st; }

    // 3. BW_RATE: data rate, normal power
    uint8_t bw = static_cast<uint8_t>(rate_);
    st = bus_.writeReg(BW_RATE_REG, &bw, 1U);
    if (st != Status::Ok) { return st; }

    // 4. Disable all interrupts
    uint8_t int_en = 0x00U;
    st = bus_.writeReg(INT_ENABLE_REG, &int_en, 1U);
    if (st != Status::Ok) { return st; }

    // 5. FIFO bypass (disabled)
    uint8_t fifo = 0x00U;
    st = bus_.writeReg(FIFO_CTL_REG, &fifo, 1U);
    if (st != Status::Ok) { return st; }

    // 6. Enable measurement mode (Measure bit = 1)
    uint8_t pwr = MEASURE_BIT;
    st = bus_.writeReg(POWER_CTL_REG, &pwr, 1U);
    if (st != Status::Ok) { return st; }

    initialized_ = true;
    return Status::Ok;
}

Status Accelerometer::reset() {
    // Put device into standby (Measure = 0); full re-init required afterward
    uint8_t pwr = 0x00U;
    Status st = bus_.writeReg(POWER_CTL_REG, &pwr, 1U);
    if (st == Status::Ok) { initialized_ = false; }
    return st;
}

bool Accelerometer::isConnected() {
    uint8_t id = 0U;
    if (bus_.readReg(DEVID_REG, &id, 1U) != Status::Ok) { return false; }
    return id == DEVID;
}

uint8_t Accelerometer::getDeviceId() { return deviceId_; }

const char* Accelerometer::getName() { return "ADXL345"; }

// ── Sensor-specific API ───────────────────────────────────────────────────

Status Accelerometer::readAcceleration(float& x, float& y, float& z) {
    if (!initialized_) { return Status::NotInitialized; }

    // Multi-byte read of DATAX0–DATAZ1 (6 bytes, auto-increment)
    uint8_t buf[6] = {};
    Status st = bus_.readReg(DATA_REG, buf, 6U);
    if (st != Status::Ok) { return st; }

    // Each axis: LSByte first, MSByte second → signed 16-bit two's complement
    int16_t rx = static_cast<int16_t>(
        static_cast<uint16_t>(buf[1]) << 8U | buf[0]);
    int16_t ry = static_cast<int16_t>(
        static_cast<uint16_t>(buf[3]) << 8U | buf[2]);
    int16_t rz = static_cast<int16_t>(
        static_cast<uint16_t>(buf[5]) << 8U | buf[4]);

    float scale = scaleFactor();
    x = static_cast<float>(rx) * scale;
    y = static_cast<float>(ry) * scale;
    z = static_cast<float>(rz) * scale;
    return Status::Ok;
}

Status Accelerometer::setRange(Range range) {
    range_ = range;
    uint8_t fmt = static_cast<uint8_t>(FULL_RES_BIT |
                                       static_cast<uint8_t>(range_));
    return bus_.writeReg(DATA_FORMAT_REG, &fmt, 1U);
}

// ── Private helpers ───────────────────────────────────────────────────────

float Accelerometer::scaleFactor() const {
    // FULL_RES mode: 3.9 mg/LSB ≈ 4 mg/LSB for all ranges
    return 0.004f;
}

} // namespace drivers
} // namespace sensor
