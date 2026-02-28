#include "drivers/pressure_sensor.hpp"

#include <cstdint>

namespace {

static uint16_t parseU16(const uint8_t* buf, size_t lo) {
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(buf[lo + 1U]) << 8U) | buf[lo]);
}

static int16_t parseS16(const uint8_t* buf, size_t lo) {
    return static_cast<int16_t>(parseU16(buf, lo));
}

static int32_t parse20bit(uint8_t msb, uint8_t lsb, uint8_t xlsb) {
    return (static_cast<int32_t>(msb) << 12) |
           (static_cast<int32_t>(lsb) << 4) |
           (static_cast<int32_t>(xlsb) >> 4);
}

} // namespace

namespace sensor {
namespace drivers {

PressureSensor::PressureSensor(II2C& bus, uint8_t addr)
    : bus_(bus), addr_(addr) {}

// ── ISensor ───────────────────────────────────────────────────────────────

Status PressureSensor::init() {
    // 1. Verify chip ID
    uint8_t id = 0U;
    Status st = bus_.readReg(addr_, CHIP_ID_REG, &id, 1U);
    if (st != Status::Ok) { return st; }
    if (id != CHIP_ID) { return Status::NotFound; }
    deviceId_ = id;

    // 2. Soft reset
    uint8_t rst = RESET_VALUE;
    st = bus_.writeReg(addr_, RESET_REG, &rst, 1U);
    if (st != Status::Ok) { return st; }

    // Wait for NVM copy to complete (STATUS[0] = im_update must be 0)
    // BMP280 NVM copy completes within 2 ms after reset (datasheet §4.1);
    // 10 retries is a bounded safety guard for the polling loop.
    {
        constexpr int k_maxRetries = 10;
        bool nvm_ready = false;
        for (int i = 0; i < k_maxRetries; ++i) {
            uint8_t s = 0U;
            st = bus_.readReg(addr_, STATUS_REG, &s, 1U);
            if (st != Status::Ok) { return st; }
            if ((s & 0x01U) == 0U) { nvm_ready = true; break; }
        }
        if (!nvm_ready) { return Status::Timeout; }
    }

    // 3. Read calibration data (0x88–0x9F, 24 bytes)
    st = readCalibration();
    if (st != Status::Ok) { return st; }

    // 4. Config: no IIR filter, 0.5 ms standby
    uint8_t cfg = 0x00U;
    st = bus_.writeReg(addr_, CONFIG_REG, &cfg, 1U);
    if (st != Status::Ok) { return st; }

    // 5. ctrl_meas: osrs_t=×1, osrs_p=×1, Normal mode (0b001 001 11 = 0x27)
    uint8_t ctrl_meas = 0x27U;
    st = bus_.writeReg(addr_, CTRL_MEAS_REG, &ctrl_meas, 1U);
    if (st != Status::Ok) { return st; }

    initialized_ = true;
    return Status::Ok;
}

Status PressureSensor::reset() {
    uint8_t rst = RESET_VALUE;
    Status st = bus_.writeReg(addr_, RESET_REG, &rst, 1U);
    if (st == Status::Ok) { initialized_ = false; }
    return st;
}

bool PressureSensor::isConnected() {
    uint8_t id = 0U;
    if (bus_.readReg(addr_, CHIP_ID_REG, &id, 1U) != Status::Ok) {
        return false;
    }
    return id == CHIP_ID;
}

uint8_t PressureSensor::getDeviceId() { return deviceId_; }

const char* PressureSensor::getName() { return "BMP280"; }

// ── Public read API ───────────────────────────────────────────────────────

Status PressureSensor::readTemperature(float& temperature) {
    if (!initialized_) { return Status::NotInitialized; }

    Status st = waitForMeasurement();
    if (st != Status::Ok) { return st; }

    uint8_t buf[3] = {};
    st = bus_.readReg(addr_, 0xFAU, buf, 3U);
    if (st != Status::Ok) { return st; }

    int32_t raw = parse20bit(buf[0], buf[1], buf[2]);
    int32_t T   = compensateTemperature(raw);
    temperature = static_cast<float>(T) / 100.0f;
    return Status::Ok;
}

Status PressureSensor::readPressure(float& pressure) {
    if (!initialized_) { return Status::NotInitialized; }

    Status st = waitForMeasurement();
    if (st != Status::Ok) { return st; }

    uint8_t buf[6] = {};
    st = bus_.readReg(addr_, DATA_REG, buf, 6U);
    if (st != Status::Ok) { return st; }

    int32_t press_raw = parse20bit(buf[0], buf[1], buf[2]);
    int32_t temp_raw  = parse20bit(buf[3], buf[4], buf[5]);

    compensateTemperature(temp_raw);           // updates t_fine_
    uint32_t P = compensatePressure(press_raw);
    pressure = static_cast<float>(P) / 256.0f; // Q24.8 → Pa
    return Status::Ok;
}

Status PressureSensor::readBoth(float& temperature, float& pressure) {
    if (!initialized_) { return Status::NotInitialized; }

    Status st = waitForMeasurement();
    if (st != Status::Ok) { return st; }

    uint8_t buf[6] = {};
    st = bus_.readReg(addr_, DATA_REG, buf, 6U);
    if (st != Status::Ok) { return st; }

    int32_t press_raw = parse20bit(buf[0], buf[1], buf[2]);
    int32_t temp_raw  = parse20bit(buf[3], buf[4], buf[5]);

    int32_t  T = compensateTemperature(temp_raw); // sets t_fine_
    uint32_t P = compensatePressure(press_raw);

    temperature = static_cast<float>(T) / 100.0f;
    pressure    = static_cast<float>(P) / 256.0f;
    return Status::Ok;
}

// ── Measurement-ready polling ─────────────────────────────────────────────

Status PressureSensor::waitForMeasurement() {
    // BMP280 worst-case measurement time is ~40 ms (datasheet §9.1);
    // 10 retries is a bounded safety guard for the polling loop.
    constexpr int k_maxRetries = 10;
    bool meas_done = false;
    for (int i = 0; i < k_maxRetries; ++i) {
        uint8_t s = 0U;
        Status st = bus_.readReg(addr_, STATUS_REG, &s, 1U);
        if (st != Status::Ok) { return st; }
        if ((s & 0x08U) == 0U) { meas_done = true; break; }
    }
    return meas_done ? Status::Ok : Status::Timeout;
}

// ── Calibration ───────────────────────────────────────────────────────────

Status PressureSensor::readCalibration() {
    // 0x88–0x9F: 24 bytes for T1-T3 and P1-P9
    uint8_t buf[24] = {};
    Status st = bus_.readReg(addr_, CALIB_REG, buf, 24U);
    if (st != Status::Ok) { return st; }

    calib_.dig_T1 = parseU16(buf, 0U);
    calib_.dig_T2 = parseS16(buf, 2U);
    calib_.dig_T3 = parseS16(buf, 4U);
    calib_.dig_P1 = parseU16(buf, 6U);
    calib_.dig_P2 = parseS16(buf, 8U);
    calib_.dig_P3 = parseS16(buf, 10U);
    calib_.dig_P4 = parseS16(buf, 12U);
    calib_.dig_P5 = parseS16(buf, 14U);
    calib_.dig_P6 = parseS16(buf, 16U);
    calib_.dig_P7 = parseS16(buf, 18U);
    calib_.dig_P8 = parseS16(buf, 20U);
    calib_.dig_P9 = parseS16(buf, 22U);

    return Status::Ok;
}

// ── Compensation algorithms (BMP280 datasheet integer versions) ───────────

int32_t PressureSensor::compensateTemperature(int32_t adcT) {
    int32_t var1 =
        ((((adcT >> 3) - (static_cast<int32_t>(calib_.dig_T1) << 1)) *
          static_cast<int32_t>(calib_.dig_T2)) >>
         11);
    int32_t var2 =
        (((((adcT >> 4) - static_cast<int32_t>(calib_.dig_T1)) *
           ((adcT >> 4) - static_cast<int32_t>(calib_.dig_T1))) >>
          12) *
         static_cast<int32_t>(calib_.dig_T3)) >>
        14;
    t_fine_ = var1 + var2;
    return (t_fine_ * 5 + 128) >> 8;
}

uint32_t PressureSensor::compensatePressure(int32_t adcP) {
    int64_t var1 = static_cast<int64_t>(t_fine_) - 128000LL;
    int64_t var2 = var1 * var1 * static_cast<int64_t>(calib_.dig_P6);
    var2 += (var1 * static_cast<int64_t>(calib_.dig_P5)) << 17;
    var2 += static_cast<int64_t>(calib_.dig_P4) << 35;
    var1 = ((var1 * var1 * static_cast<int64_t>(calib_.dig_P3)) >> 8) +
           ((var1 * static_cast<int64_t>(calib_.dig_P2)) << 12);
    var1 = ((static_cast<int64_t>(1) << 47) + var1) *
           static_cast<int64_t>(calib_.dig_P1) >> 33;
    if (var1 == 0LL) { return 0U; }
    int64_t p = 1048576LL - static_cast<int64_t>(adcP);
    p = (((p << 31) - var2) * 3125LL) / var1;
    var1 = (static_cast<int64_t>(calib_.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (static_cast<int64_t>(calib_.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) +
        (static_cast<int64_t>(calib_.dig_P7) << 4);
    return static_cast<uint32_t>(p);
}

} // namespace drivers
} // namespace sensor
