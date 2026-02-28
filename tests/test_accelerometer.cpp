#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "drivers/accelerometer.hpp"

using namespace sensor;
using namespace sensor::drivers;
using ::testing::_;
using ::testing::Return;

// ── Mock ──────────────────────────────────────────────────────────────────

class MockSPI : public ISPI {
public:
    MOCK_METHOD(Status, writeReg,
                (uint8_t regAddr, const uint8_t* data, size_t len),
                (override));
    MOCK_METHOD(Status, readReg,
                (uint8_t regAddr, uint8_t* data, size_t len),
                (override));
};

// ── Test fixture ──────────────────────────────────────────────────────────

class AccelerometerTest : public ::testing::Test {
protected:
    MockSPI      mock_;
    Accelerometer sensor_{mock_};

    void expectDevIdRead() {
        EXPECT_CALL(mock_, readReg(Accelerometer::DEVID_REG, _, 1U))
            .WillOnce([](uint8_t, uint8_t* buf, size_t) {
                buf[0] = Accelerometer::DEVID;
                return Status::Ok;
            });
    }

    void expectDataFormatWrite() {
        EXPECT_CALL(mock_, writeReg(Accelerometer::DATA_FORMAT_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void expectBwRateWrite() {
        EXPECT_CALL(mock_, writeReg(Accelerometer::BW_RATE_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void expectIntEnableWrite() {
        EXPECT_CALL(mock_, writeReg(Accelerometer::INT_ENABLE_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void expectFifoCtlWrite() {
        EXPECT_CALL(mock_, writeReg(Accelerometer::FIFO_CTL_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void expectPowerCtlWrite() {
        EXPECT_CALL(mock_, writeReg(Accelerometer::POWER_CTL_REG, _, 1U))
            .WillOnce(Return(Status::Ok));
    }

    void initSuccessfully() {
        expectDevIdRead();
        expectDataFormatWrite();
        expectBwRateWrite();
        expectIntEnableWrite();
        expectFifoCtlWrite();
        expectPowerCtlWrite();
        ASSERT_EQ(Status::Ok, sensor_.init());
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────

TEST_F(AccelerometerTest, GetName_ReturnsADXL345) {
    EXPECT_STREQ("ADXL345", sensor_.getName());
}

TEST_F(AccelerometerTest, Init_Success) {
    initSuccessfully();
    EXPECT_EQ(Accelerometer::DEVID, sensor_.getDeviceId());
}

TEST_F(AccelerometerTest, Init_WrongDeviceId_ReturnsNotFound) {
    EXPECT_CALL(mock_, readReg(Accelerometer::DEVID_REG, _, 1U))
        .WillOnce([](uint8_t, uint8_t* buf, size_t) {
            buf[0] = 0x00U;
            return Status::Ok;
        });
    EXPECT_EQ(Status::NotFound, sensor_.init());
}

TEST_F(AccelerometerTest, Init_BusError_PropagatesError) {
    EXPECT_CALL(mock_, readReg(Accelerometer::DEVID_REG, _, 1U))
        .WillOnce(Return(Status::Error));
    EXPECT_EQ(Status::Error, sensor_.init());
}

TEST_F(AccelerometerTest, IsConnected_AfterInit_ReturnsTrue) {
    initSuccessfully();
    EXPECT_CALL(mock_, readReg(Accelerometer::DEVID_REG, _, 1U))
        .WillOnce([](uint8_t, uint8_t* buf, size_t) {
            buf[0] = Accelerometer::DEVID;
            return Status::Ok;
        });
    EXPECT_TRUE(sensor_.isConnected());
}

TEST_F(AccelerometerTest, IsConnected_BusError_ReturnsFalse) {
    EXPECT_CALL(mock_, readReg(Accelerometer::DEVID_REG, _, 1U))
        .WillOnce(Return(Status::Error));
    EXPECT_FALSE(sensor_.isConnected());
}

TEST_F(AccelerometerTest, GetDeviceId_BeforeInit_ReturnsZero) {
    EXPECT_EQ(0U, sensor_.getDeviceId());
}

TEST_F(AccelerometerTest, Reset_SetsMeasureBitToZero) {
    EXPECT_CALL(mock_, writeReg(Accelerometer::POWER_CTL_REG, _, 1U))
        .WillOnce([](uint8_t, const uint8_t* data, size_t) {
            EXPECT_EQ(0x00U, data[0]);
            return Status::Ok;
        });
    EXPECT_EQ(Status::Ok, sensor_.reset());
}

TEST_F(AccelerometerTest, ReadAcceleration_BeforeInit_ReturnsNotInitialized) {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    EXPECT_EQ(Status::NotInitialized, sensor_.readAcceleration(x, y, z));
}

TEST_F(AccelerometerTest, ReadAcceleration_ZeroData_ReturnsZeroG) {
    initSuccessfully();
    uint8_t raw[6] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    EXPECT_CALL(mock_, readReg(Accelerometer::DATA_REG, _, 6U))
        .WillOnce([&raw](uint8_t, uint8_t* buf, size_t len) {
            for (size_t i = 0; i < len; ++i) { buf[i] = raw[i]; }
            return Status::Ok;
        });
    float x = 1.0f, y = 1.0f, z = 1.0f;
    EXPECT_EQ(Status::Ok, sensor_.readAcceleration(x, y, z));
    EXPECT_FLOAT_EQ(0.0f, x);
    EXPECT_FLOAT_EQ(0.0f, y);
    EXPECT_FLOAT_EQ(0.0f, z);
}

TEST_F(AccelerometerTest, ReadAcceleration_PositiveX_CorrectScale) {
    initSuccessfully();
    // raw X = 100 LSBs → 100 × 0.004 g = 0.4 g
    // little-endian: LSB=100 (0x64), MSB=0x00
    uint8_t raw[6] = {0x64U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    EXPECT_CALL(mock_, readReg(Accelerometer::DATA_REG, _, 6U))
        .WillOnce([&raw](uint8_t, uint8_t* buf, size_t len) {
            for (size_t i = 0; i < len; ++i) { buf[i] = raw[i]; }
            return Status::Ok;
        });
    float x = 0.0f, y = 0.0f, z = 0.0f;
    EXPECT_EQ(Status::Ok, sensor_.readAcceleration(x, y, z));
    EXPECT_NEAR(0.4f, x, 0.001f);
    EXPECT_FLOAT_EQ(0.0f, y);
    EXPECT_FLOAT_EQ(0.0f, z);
}

TEST_F(AccelerometerTest, ReadAcceleration_NegativeZ_CorrectScale) {
    initSuccessfully();
    // raw Z = -256 (0xFF00 in little-endian, two's complement)
    // -256 × 0.004 g = -1.024 g
    uint8_t raw[6] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xFFU};
    EXPECT_CALL(mock_, readReg(Accelerometer::DATA_REG, _, 6U))
        .WillOnce([&raw](uint8_t, uint8_t* buf, size_t len) {
            for (size_t i = 0; i < len; ++i) { buf[i] = raw[i]; }
            return Status::Ok;
        });
    float x = 0.0f, y = 0.0f, z = 0.0f;
    EXPECT_EQ(Status::Ok, sensor_.readAcceleration(x, y, z));
    EXPECT_FLOAT_EQ(0.0f, x);
    EXPECT_FLOAT_EQ(0.0f, y);
    EXPECT_NEAR(-1.024f, z, 0.001f);
}

TEST_F(AccelerometerTest, SetRange_WritesDataFormatRegister) {
    // setRange does not require prior init
    EXPECT_CALL(mock_, writeReg(Accelerometer::DATA_FORMAT_REG, _, 1U))
        .WillOnce([](uint8_t, const uint8_t* data, size_t) {
            // FULL_RES=0x08 | G16=0x03 = 0x0B
            EXPECT_EQ(0x0BU, data[0]);
            return Status::Ok;
        });
    EXPECT_EQ(Status::Ok, sensor_.setRange(Accelerometer::Range::G16));
}

TEST_F(AccelerometerTest, Init_SetsMeasureBitInPowerCtl) {
    expectDevIdRead();
    expectDataFormatWrite();
    expectBwRateWrite();
    expectIntEnableWrite();
    expectFifoCtlWrite();
    EXPECT_CALL(mock_, writeReg(Accelerometer::POWER_CTL_REG, _, 1U))
        .WillOnce([](uint8_t, const uint8_t* data, size_t) {
            EXPECT_EQ(Accelerometer::MEASURE_BIT, data[0]);
            return Status::Ok;
        });
    EXPECT_EQ(Status::Ok, sensor_.init());
}
