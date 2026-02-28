#include "drivers/temperature_sensor.hpp"

#include <cstdint>

namespace {

// Little-endian 16-bit helpers
static uint16_t parseU16(const uint8_t* buf, size_t lo) {
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(buf[lo + 1U]) << 8U) | buf[lo]);
}

static int16_t parseS16(const uint8_t* buf, size_t lo) {
    return static_cast<int16_t>(parseU16(buf, lo));
}

// 20-bit raw sensor value from MSB/LSB/XLSB bytes
static int32_t parse20bit(uint8_t msb, uint8_t lsb, uint8_t xlsb) {
    return (static_cast<int32_t>(msb) << 12) |
           (static_cast<int32_t>(lsb) << 4) |
           (static_cast<int32_t>(xlsb) >> 4);
}

} // namespace

namespace sensor {
namespace drivers {

TemperatureSensor::TemperatureSensor(II2C& bus, uint8_t addr)
    : bus_(bus), addr_(addr) {}

// ── ISensor ───────────────────────────────────────────────────────────────

Status TemperatureSensor::init() {
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

    // 3. Read calibration data
    st = readCalibration();
    if (st != Status::Ok) { return st; }

    // 4. Humidity oversampling ×1 (must be written before ctrl_meas)
    uint8_t ctrl_hum = 0x01U;
    st = bus_.writeReg(addr_, CTRL_HUM_REG, &ctrl_hum, 1U);
    if (st != Status::Ok) { return st; }

    // 5. Config: no IIR filter, 0.5 ms standby
    uint8_t cfg = 0x00U;
    st = bus_.writeReg(addr_, CONFIG_REG, &cfg, 1U);
    if (st != Status::Ok) { return st; }

    // 6. ctrl_meas: osrs_t=×1, osrs_p=×1, Normal mode
    //    0b 001 001 11  = 0x27
    uint8_t ctrl_meas = 0x27U;
    st = bus_.writeReg(addr_, CTRL_MEAS_REG, &ctrl_meas, 1U);
    if (st != Status::Ok) { return st; }

    initialized_ = true;
    return Status::Ok;
}

Status TemperatureSensor::reset() {
    uint8_t rst = RESET_VALUE;
    Status st = bus_.writeReg(addr_, RESET_REG, &rst, 1U);
    if (st == Status::Ok) { initialized_ = false; }
    return st;
}

bool TemperatureSensor::isConnected() {
    uint8_t id = 0U;
    if (bus_.readReg(addr_, CHIP_ID_REG, &id, 1U) != Status::Ok) {
        return false;
    }
    return id == CHIP_ID;
}

uint8_t TemperatureSensor::getDeviceId() { return deviceId_; }

const char* TemperatureSensor::getName() { return "BME280"; }

// ── Public read API ───────────────────────────────────────────────────────

Status TemperatureSensor::readTemperature(float& temperature) {
    if (!initialized_) { return Status::NotInitialized; }

    // Read only temperature bytes: temp_msb/lsb/xlsb at 0xFA–0xFC
    uint8_t buf[3] = {};
    Status st = bus_.readReg(addr_, 0xFAU, buf, 3U);
    if (st != Status::Ok) { return st; }

    int32_t raw = parse20bit(buf[0], buf[1], buf[2]);
    int32_t T   = compensateTemperature(raw);
    temperature = static_cast<float>(T) / 100.0f;
    return Status::Ok;
}

Status TemperatureSensor::readPressure(float& pressure) {
    if (!initialized_) { return Status::NotInitialized; }

    // Read 6 bytes (press + temp) — temperature needed to update t_fine
    uint8_t buf[6] = {};
    Status st = bus_.readReg(addr_, DATA_REG, buf, 6U);
    if (st != Status::Ok) { return st; }

    int32_t press_raw = parse20bit(buf[0], buf[1], buf[2]);
    int32_t temp_raw  = parse20bit(buf[3], buf[4], buf[5]);

    compensateTemperature(temp_raw);           // updates t_fine_
    uint32_t P = compensatePressure(press_raw);
    pressure = static_cast<float>(P) / 256.0f; // Q24.8 → Pa
    return Status::Ok;
}

Status TemperatureSensor::readHumidity(float& humidity) {
    if (!initialized_) { return Status::NotInitialized; }

    // Read all 8 bytes — temperature needed to update t_fine
    uint8_t buf[8] = {};
    Status st = bus_.readReg(addr_, DATA_REG, buf, 8U);
    if (st != Status::Ok) { return st; }

    int32_t temp_raw = parse20bit(buf[3], buf[4], buf[5]);
    int32_t hum_raw  = (static_cast<int32_t>(buf[6]) << 8) |
                        static_cast<int32_t>(buf[7]);

    compensateTemperature(temp_raw);           // updates t_fine_
    uint32_t H = compensateHumidity(hum_raw);
    humidity = static_cast<float>(H) / 1024.0f; // Q22.10 → %RH
    return Status::Ok;
}

Status TemperatureSensor::readAll(float& temperature, float& pressure,
                                  float& humidity) {
    if (!initialized_) { return Status::NotInitialized; }

    uint8_t buf[8] = {};
    Status st = bus_.readReg(addr_, DATA_REG, buf, 8U);
    if (st != Status::Ok) { return st; }

    int32_t press_raw = parse20bit(buf[0], buf[1], buf[2]);
    int32_t temp_raw  = parse20bit(buf[3], buf[4], buf[5]);
    int32_t hum_raw   = (static_cast<int32_t>(buf[6]) << 8) |
                         static_cast<int32_t>(buf[7]);

    int32_t  T = compensateTemperature(temp_raw); // sets t_fine_
    uint32_t P = compensatePressure(press_raw);
    uint32_t H = compensateHumidity(hum_raw);

    temperature = static_cast<float>(T) / 100.0f;
    pressure    = static_cast<float>(P) / 256.0f;
    humidity    = static_cast<float>(H) / 1024.0f;
    return Status::Ok;
}

// ── Calibration ───────────────────────────────────────────────────────────

Status TemperatureSensor::readCalibration() {
    // 0x88–0xA1: 26 bytes covering T1-T3, P1-P9 (0x88-0x9F),
    //            reserved 0xA0, and dig_H1 at 0xA1
    uint8_t tp[26] = {};
    Status st = bus_.readReg(addr_, CALIB_TP_REG, tp, 26U);
    if (st != Status::Ok) { return st; }

    calib_.dig_T1 = parseU16(tp, 0U);
    calib_.dig_T2 = parseS16(tp, 2U);
    calib_.dig_T3 = parseS16(tp, 4U);
    calib_.dig_P1 = parseU16(tp, 6U);
    calib_.dig_P2 = parseS16(tp, 8U);
    calib_.dig_P3 = parseS16(tp, 10U);
    calib_.dig_P4 = parseS16(tp, 12U);
    calib_.dig_P5 = parseS16(tp, 14U);
    calib_.dig_P6 = parseS16(tp, 16U);
    calib_.dig_P7 = parseS16(tp, 18U);
    calib_.dig_P8 = parseS16(tp, 20U);
    calib_.dig_P9 = parseS16(tp, 22U);
    // tp[24] = register 0xA0 (reserved/unused)
    calib_.dig_H1 = tp[25];  // register 0xA1

    // 0xE1–0xE7: 7 bytes for dig_H2..dig_H6
    uint8_t hb[7] = {};
    st = bus_.readReg(addr_, CALIB_H2_REG, hb, 7U);
    if (st != Status::Ok) { return st; }

    calib_.dig_H2 = parseS16(hb, 0U);
    calib_.dig_H3 = hb[2];
    // dig_H4: upper 8 bits from 0xE4, lower 4 bits from 0xE5[3:0]
    calib_.dig_H4 = static_cast<int16_t>(
        (static_cast<int16_t>(hb[3]) << 4) |
        static_cast<int16_t>(hb[4] & 0x0FU));
    // dig_H5: lower 4 bits from 0xE5[7:4], upper 8 bits from 0xE6
    calib_.dig_H5 = static_cast<int16_t>(
        (static_cast<int16_t>(hb[5]) << 4) |
        static_cast<int16_t>(hb[4] >> 4U));
    calib_.dig_H6 = static_cast<int8_t>(hb[6]);

    return Status::Ok;
}

// ── Compensation algorithms (BME280 datasheet integer versions) ───────────

int32_t TemperatureSensor::compensateTemperature(int32_t adcT) {
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

uint32_t TemperatureSensor::compensatePressure(int32_t adcP) {
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

uint32_t TemperatureSensor::compensateHumidity(int32_t adcH) {
    int32_t v = t_fine_ - 76800;
    v = (((((adcH << 14) -
            (static_cast<int32_t>(calib_.dig_H4) << 20) -
            (static_cast<int32_t>(calib_.dig_H5) * v)) +
           16384) >>
          15) *
         (((((((v * static_cast<int32_t>(calib_.dig_H6)) >> 10) *
              (((v * static_cast<int32_t>(calib_.dig_H3)) >> 11) + 32768)) >>
             10) +
            2097152) *
           static_cast<int32_t>(calib_.dig_H2) +
           8192) >>
          14));
    v -= (((((v >> 15) * (v >> 15)) >> 7) *
           static_cast<int32_t>(calib_.dig_H1)) >>
          4);
    v = (v < 0) ? 0 : v;
    v = (v > 419430400) ? 419430400 : v;
    return static_cast<uint32_t>(v >> 12);
}

} // namespace drivers
} // namespace sensor
