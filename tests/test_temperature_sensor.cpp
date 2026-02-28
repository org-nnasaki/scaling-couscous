#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "drivers/temperature_sensor.hpp"

using namespace sensor;
using namespace sensor::drivers;
using ::testing::_;
using ::testing::Return;

// ── Mock ──────────────────────────────────────────────────────────────────

class MockI2C : public II2C {
public:
    MOCK_METHOD(Status, writeReg,
                (uint8_t devAddr, uint8_t regAddr,
                 const uint8_t* data, size_t len),
                (override));
    MOCK_METHOD(Status, readReg,
                (uint8_t devAddr, uint8_t regAddr,
                 uint8_t* data, size_t len),
                (override));
};

// ── Test fixture ──────────────────────────────────────────────────────────

class TemperatureSensorTest : public ::testing::Test {
protected:
    static constexpr uint8_t ADDR = TemperatureSensor::DEFAULT_I2C_ADDR;

    MockI2C              mock_;
    TemperatureSensor    sensor_{mock_};

    // Realistic calibration values (from BME280 datasheet example)
    // dig_T1=27504, dig_T2=26435, dig_T3=-1000,
    // dig_P1=36477, dig_P2=-10685, ... (others zeroed for simplicity)
    static constexpr uint8_t k_calib_tp[26] = {
        // T1=27504 (0x6B70) little-endian
        0x70U, 0x6BU,
        // T2=26435 (0x6743) little-endian
        0x43U, 0x67U,
        // T3=-1000 (0xFC18) little-endian
        0x18U, 0xFCU,
        // P1=36477 (0x8E7D) little-endian
        0x7DU, 0x8EU,
        // P2=-10685 (0xD643) little-endian
        0x43U, 0xD6U,
        // P3..P9: zero-fill
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U,
        // 0xA0 reserved
        0x00U,
        // dig_H1 = 75
        75U
    };

    static constexpr uint8_t k_calib_h[7] = {
        // dig_H2=372 (0x0174), dig_H3=0, dig_H4=30, dig_H5=3, dig_H6=30
        0x74U, 0x01U, 0x00U, 0x1EU, 0x30U, 0x03U, 0x1EU
    };

    void expectChipIdRead() {
        EXPECT_CALL(mock_, readReg(ADDR, TemperatureSensor::CHIP_ID_REG, _, 1U))
            .WillOnce([](uint8_t, uint8_t, uint8_t* buf, size_t) {
                buf[0] = TemperatureSensor::CHIP_ID;
                return Status::Ok;
            });
    }

    void expectSoftReset() {
        EXPECT_CALL(mock_, writeReg(ADDR, TemperatureSensor::RESET_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void expectStatusReady() {
        EXPECT_CALL(mock_, readReg(ADDR, TemperatureSensor::STATUS_REG, _, 1U))
            .WillOnce([](uint8_t, uint8_t, uint8_t* buf, size_t) {
                buf[0] = 0x00U; // im_update = 0, NVM copy done
                return Status::Ok;
            });
    }

    void expectCalibrationRead() {
        const uint8_t* tp = k_calib_tp;
        EXPECT_CALL(mock_, readReg(ADDR, TemperatureSensor::CALIB_TP_REG, _, 26U))
            .WillOnce([tp](uint8_t, uint8_t, uint8_t* buf, size_t len) {
                for (size_t i = 0; i < len; ++i) { buf[i] = tp[i]; }
                return Status::Ok;
            });
        const uint8_t* hb = k_calib_h;
        EXPECT_CALL(mock_, readReg(ADDR, TemperatureSensor::CALIB_H2_REG, _, 7U))
            .WillOnce([hb](uint8_t, uint8_t, uint8_t* buf, size_t len) {
                for (size_t i = 0; i < len; ++i) { buf[i] = hb[i]; }
                return Status::Ok;
            });
    }

    void expectCtrlHumWrite() {
        EXPECT_CALL(mock_, writeReg(ADDR, TemperatureSensor::CTRL_HUM_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void expectConfigWrite() {
        EXPECT_CALL(mock_, writeReg(ADDR, TemperatureSensor::CONFIG_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void expectCtrlMeasWrite() {
        EXPECT_CALL(mock_, writeReg(ADDR, TemperatureSensor::CTRL_MEAS_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void initSuccessfully() {
        expectChipIdRead();
        expectSoftReset();
        expectStatusReady();
        expectCalibrationRead();
        expectCtrlHumWrite();
        expectConfigWrite();
        expectCtrlMeasWrite();
        ASSERT_EQ(Status::Ok, sensor_.init());
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────

TEST_F(TemperatureSensorTest, GetName_ReturnsBME280) {
    EXPECT_STREQ("BME280", sensor_.getName());
}

TEST_F(TemperatureSensorTest, Init_Success) {
    initSuccessfully();
    EXPECT_EQ(TemperatureSensor::CHIP_ID, sensor_.getDeviceId());
}

TEST_F(TemperatureSensorTest, Init_WrongChipId_ReturnsNotFound) {
    EXPECT_CALL(mock_, readReg(ADDR, TemperatureSensor::CHIP_ID_REG, _, 1U))
        .WillOnce([](uint8_t, uint8_t, uint8_t* buf, size_t) {
            buf[0] = 0x00U;
            return Status::Ok;
        });
    EXPECT_EQ(Status::NotFound, sensor_.init());
}

TEST_F(TemperatureSensorTest, Init_BusError_PropagatesError) {
    EXPECT_CALL(mock_, readReg(ADDR, TemperatureSensor::CHIP_ID_REG, _, 1U))
        .WillOnce(Return(Status::Error));
    EXPECT_EQ(Status::Error, sensor_.init());
}

TEST_F(TemperatureSensorTest, IsConnected_AfterInit_ReturnsTrue) {
    initSuccessfully();
    EXPECT_CALL(mock_, readReg(ADDR, TemperatureSensor::CHIP_ID_REG, _, 1U))
        .WillOnce([](uint8_t, uint8_t, uint8_t* buf, size_t) {
            buf[0] = TemperatureSensor::CHIP_ID;
            return Status::Ok;
        });
    EXPECT_TRUE(sensor_.isConnected());
}

TEST_F(TemperatureSensorTest, IsConnected_BusError_ReturnsFalse) {
    EXPECT_CALL(mock_, readReg(ADDR, TemperatureSensor::CHIP_ID_REG, _, 1U))
        .WillOnce(Return(Status::Error));
    EXPECT_FALSE(sensor_.isConnected());
}

TEST_F(TemperatureSensorTest, GetDeviceId_BeforeInit_ReturnsZero) {
    EXPECT_EQ(0U, sensor_.getDeviceId());
}

TEST_F(TemperatureSensorTest, Reset_WritesResetValue) {
    EXPECT_CALL(mock_, writeReg(ADDR, TemperatureSensor::RESET_REG, _, 1U))
        .WillOnce([](uint8_t, uint8_t, const uint8_t* data, size_t) {
            EXPECT_EQ(TemperatureSensor::RESET_VALUE, data[0]);
            return Status::Ok;
        });
    EXPECT_EQ(Status::Ok, sensor_.reset());
}

TEST_F(TemperatureSensorTest, ReadTemperature_BeforeInit_ReturnsNotInitialized) {
    float t = 0.0f;
    EXPECT_EQ(Status::NotInitialized, sensor_.readTemperature(t));
}

TEST_F(TemperatureSensorTest, ReadTemperature_Success_ReturnsReasonableValue) {
    initSuccessfully();

    // Encode adc_T ≈ 519888 → ~25 °C with reference calibration
    // temp_msb=0x7E, temp_lsb=0xC6, temp_xlsb=0x00  (raw=0x7EC60=519264)
    uint8_t raw_data[3] = {0x7EU, 0xC6U, 0x00U};
    EXPECT_CALL(mock_, readReg(ADDR, 0xFAU, _, 3U))
        .WillOnce([&raw_data](uint8_t, uint8_t, uint8_t* buf, size_t len) {
            for (size_t i = 0; i < len; ++i) { buf[i] = raw_data[i]; }
            return Status::Ok;
        });

    float temp = 0.0f;
    EXPECT_EQ(Status::Ok, sensor_.readTemperature(temp));
    // Value should be in the sensor's valid range
    EXPECT_GT(temp, -40.0f);
    EXPECT_LT(temp,  85.0f);
}

TEST_F(TemperatureSensorTest, ReadAll_Success_ReturnsAllValues) {
    initSuccessfully();

    // 8 bytes: press (3) + temp (3) + hum (2)
    uint8_t raw_data[8] = {
        // press: 0x51, 0x5D, 0x00
        0x51U, 0x5DU, 0x00U,
        // temp: 0x7E, 0xC6, 0x00
        0x7EU, 0xC6U, 0x00U,
        // hum: 0x7A, 0xE0
        0x7AU, 0xE0U
    };
    EXPECT_CALL(mock_, readReg(ADDR, TemperatureSensor::DATA_REG, _, 8U))
        .WillOnce([&raw_data](uint8_t, uint8_t, uint8_t* buf, size_t len) {
            for (size_t i = 0; i < len; ++i) { buf[i] = raw_data[i]; }
            return Status::Ok;
        });

    float temp = 0.0f, press = 0.0f, hum = 0.0f;
    EXPECT_EQ(Status::Ok, sensor_.readAll(temp, press, hum));
    EXPECT_GT(temp, -40.0f);
    EXPECT_LT(temp,  85.0f);
    EXPECT_GE(hum, 0.0f);
    EXPECT_LE(hum, 100.0f);
}

TEST_F(TemperatureSensorTest, CalibrationAccessor_ReturnsStoredValues) {
    initSuccessfully();
    // dig_T1 = 0x6B70 = 27504
    EXPECT_EQ(27504U, sensor_.calibration().dig_T1);
    // dig_H1 = 75
    EXPECT_EQ(75U, sensor_.calibration().dig_H1);
}
