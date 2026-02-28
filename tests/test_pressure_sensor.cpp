#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "drivers/pressure_sensor.hpp"

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

class PressureSensorTest : public ::testing::Test {
protected:
    static constexpr uint8_t ADDR = PressureSensor::DEFAULT_I2C_ADDR;

    MockI2C       mock_;
    PressureSensor sensor_{mock_};

    // Reference calibration (same T1-T3/P1-P9 layout as BMP280 spec)
    // dig_T1=27504, dig_T2=26435, dig_T3=-1000,
    // dig_P1=36477, dig_P2=-10749, others zeroed
    static constexpr uint8_t k_calib[24] = {
        0x70U, 0x6BU,  // T1 = 27504
        0x43U, 0x67U,  // T2 = 26435
        0x18U, 0xFCU,  // T3 = -1000
        0x7DU, 0x8EU,  // P1 = 36477
        0x03U, 0xD6U,  // P2 = -10749
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U
    };

    void expectChipIdRead() {
        EXPECT_CALL(mock_, readReg(ADDR, PressureSensor::CHIP_ID_REG, _, 1U))
            .WillOnce([](uint8_t, uint8_t, uint8_t* buf, size_t) {
                buf[0] = PressureSensor::CHIP_ID;
                return Status::Ok;
            });
    }

    void expectSoftReset() {
        EXPECT_CALL(mock_, writeReg(ADDR, PressureSensor::RESET_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void expectStatusReady() {
        EXPECT_CALL(mock_, readReg(ADDR, PressureSensor::STATUS_REG, _, 1U))
            .WillOnce([](uint8_t, uint8_t, uint8_t* buf, size_t) {
                buf[0] = 0x00U; // im_update = 0, NVM copy done
                return Status::Ok;
            });
    }

    void expectCalibrationRead() {
        const uint8_t* cal = k_calib;
        EXPECT_CALL(mock_, readReg(ADDR, PressureSensor::CALIB_REG, _, 24U))
            .WillOnce([cal](uint8_t, uint8_t, uint8_t* buf, size_t len) {
                for (size_t i = 0; i < len; ++i) { buf[i] = cal[i]; }
                return Status::Ok;
            });
    }

    void expectConfigWrite() {
        EXPECT_CALL(mock_, writeReg(ADDR, PressureSensor::CONFIG_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void expectCtrlMeasWrite() {
        EXPECT_CALL(mock_, writeReg(ADDR, PressureSensor::CTRL_MEAS_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void initSuccessfully() {
        expectChipIdRead();
        expectSoftReset();
        expectStatusReady();
        expectCalibrationRead();
        expectConfigWrite();
        expectCtrlMeasWrite();
        ASSERT_EQ(Status::Ok, sensor_.init());
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────

TEST_F(PressureSensorTest, GetName_ReturnsBMP280) {
    EXPECT_STREQ("BMP280", sensor_.getName());
}

TEST_F(PressureSensorTest, Init_Success) {
    initSuccessfully();
    EXPECT_EQ(PressureSensor::CHIP_ID, sensor_.getDeviceId());
}

TEST_F(PressureSensorTest, Init_WrongChipId_ReturnsNotFound) {
    EXPECT_CALL(mock_, readReg(ADDR, PressureSensor::CHIP_ID_REG, _, 1U))
        .WillOnce([](uint8_t, uint8_t, uint8_t* buf, size_t) {
            buf[0] = 0x60U; // BME280 id, not BMP280
            return Status::Ok;
        });
    EXPECT_EQ(Status::NotFound, sensor_.init());
}

TEST_F(PressureSensorTest, Init_BusError_PropagatesError) {
    EXPECT_CALL(mock_, readReg(ADDR, PressureSensor::CHIP_ID_REG, _, 1U))
        .WillOnce(Return(Status::Error));
    EXPECT_EQ(Status::Error, sensor_.init());
}

TEST_F(PressureSensorTest, IsConnected_AfterInit_ReturnsTrue) {
    initSuccessfully();
    EXPECT_CALL(mock_, readReg(ADDR, PressureSensor::CHIP_ID_REG, _, 1U))
        .WillOnce([](uint8_t, uint8_t, uint8_t* buf, size_t) {
            buf[0] = PressureSensor::CHIP_ID;
            return Status::Ok;
        });
    EXPECT_TRUE(sensor_.isConnected());
}

TEST_F(PressureSensorTest, IsConnected_BusError_ReturnsFalse) {
    EXPECT_CALL(mock_, readReg(ADDR, PressureSensor::CHIP_ID_REG, _, 1U))
        .WillOnce(Return(Status::Error));
    EXPECT_FALSE(sensor_.isConnected());
}

TEST_F(PressureSensorTest, GetDeviceId_BeforeInit_ReturnsZero) {
    EXPECT_EQ(0U, sensor_.getDeviceId());
}

TEST_F(PressureSensorTest, Reset_WritesResetValue) {
    EXPECT_CALL(mock_, writeReg(ADDR, PressureSensor::RESET_REG, _, 1U))
        .WillOnce([](uint8_t, uint8_t, const uint8_t* data, size_t) {
            EXPECT_EQ(PressureSensor::RESET_VALUE, data[0]);
            return Status::Ok;
        });
    EXPECT_EQ(Status::Ok, sensor_.reset());
}

TEST_F(PressureSensorTest, ReadTemperature_BeforeInit_ReturnsNotInitialized) {
    float t = 0.0f;
    EXPECT_EQ(Status::NotInitialized, sensor_.readTemperature(t));
}

TEST_F(PressureSensorTest, ReadPressure_BeforeInit_ReturnsNotInitialized) {
    float p = 0.0f;
    EXPECT_EQ(Status::NotInitialized, sensor_.readPressure(p));
}

TEST_F(PressureSensorTest, ReadTemperature_Success_ReturnsReasonableValue) {
    initSuccessfully();

    // STATUS: not measuring
    EXPECT_CALL(mock_, readReg(ADDR, PressureSensor::STATUS_REG, _, 1U))
        .WillOnce([](uint8_t, uint8_t, uint8_t* buf, size_t) {
            buf[0] = 0x00U;
            return Status::Ok;
        });

    // Same raw temp bytes as temperature sensor test
    uint8_t raw[3] = {0x7EU, 0xC6U, 0x00U};
    EXPECT_CALL(mock_, readReg(ADDR, 0xFAU, _, 3U))
        .WillOnce([&raw](uint8_t, uint8_t, uint8_t* buf, size_t len) {
            for (size_t i = 0; i < len; ++i) { buf[i] = raw[i]; }
            return Status::Ok;
        });

    float temp = 0.0f;
    EXPECT_EQ(Status::Ok, sensor_.readTemperature(temp));
    EXPECT_GT(temp, -40.0f);
    EXPECT_LT(temp,  85.0f);
}

TEST_F(PressureSensorTest, ReadBoth_Success_ReturnsBothValues) {
    initSuccessfully();

    // STATUS: not measuring
    EXPECT_CALL(mock_, readReg(ADDR, PressureSensor::STATUS_REG, _, 1U))
        .WillOnce([](uint8_t, uint8_t, uint8_t* buf, size_t) {
            buf[0] = 0x00U;
            return Status::Ok;
        });

    uint8_t raw[6] = {
        0x51U, 0x5DU, 0x00U,  // press
        0x7EU, 0xC6U, 0x00U   // temp
    };
    EXPECT_CALL(mock_, readReg(ADDR, PressureSensor::DATA_REG, _, 6U))
        .WillOnce([&raw](uint8_t, uint8_t, uint8_t* buf, size_t len) {
            for (size_t i = 0; i < len; ++i) { buf[i] = raw[i]; }
            return Status::Ok;
        });

    float temp = 0.0f, press = 0.0f;
    EXPECT_EQ(Status::Ok, sensor_.readBoth(temp, press));
    EXPECT_GT(temp, -40.0f);
    EXPECT_LT(temp,  85.0f);
    EXPECT_GE(press, 0.0f);
}

TEST_F(PressureSensorTest, CalibrationAccessor_ReturnsStoredValues) {
    initSuccessfully();
    EXPECT_EQ(27504U, sensor_.calibration().dig_T1);
    EXPECT_EQ(26435,  sensor_.calibration().dig_T2);
}

TEST_F(PressureSensorTest, ChipIdDistinctFromBME280) {
    // BMP280 chip ID (0x58) must differ from BME280 (0x60)
    EXPECT_NE(PressureSensor::CHIP_ID, 0x60U);
    EXPECT_EQ(PressureSensor::CHIP_ID, 0x58U);
}
