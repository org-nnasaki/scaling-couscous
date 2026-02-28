/// examples/main.cpp
/// Demonstrates how to use all three sensor drivers with stub HAL implementations.
/// In production, replace StubI2C / StubSPI with real platform drivers.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <array>

#include "drivers/temperature_sensor.hpp"
#include "drivers/pressure_sensor.hpp"
#include "drivers/accelerometer.hpp"

// ── Stub HAL implementations (simulate connected sensors) ─────────────────

class StubI2C : public sensor::II2C {
public:
    sensor::Status writeReg(uint8_t /*devAddr*/, uint8_t /*regAddr*/,
                            const uint8_t* /*data*/, size_t /*len*/) override {
        return sensor::Status::Ok;
    }

    sensor::Status readReg(uint8_t devAddr, uint8_t regAddr,
                           uint8_t* data, size_t len) override {
        using namespace sensor::drivers;
        std::memset(data, 0, len);

        // Return the correct chip ID depending on I2C address:
        //   0x76 → BME280 (0x60), 0x77 → BMP280 (0x58)
        if (regAddr == 0xD0U && len >= 1U) {
            data[0] = (devAddr == 0x77U)
                      ? PressureSensor::CHIP_ID      // BMP280 = 0x58
                      : TemperatureSensor::CHIP_ID;  // BME280 = 0x60
        }

        // Return plausible temperature raw bytes at 0xFA (≈25 °C)
        if (regAddr == 0xFAU) {
            data[0] = 0x7EU; data[1] = 0xC6U; if (len > 2U) { data[2] = 0x00U; }
        }
        return sensor::Status::Ok;
    }
};

class StubSPI : public sensor::ISPI {
public:
    sensor::Status writeReg(uint8_t /*regAddr*/,
                            const uint8_t* /*data*/, size_t /*len*/) override {
        return sensor::Status::Ok;
    }

    sensor::Status readReg(uint8_t regAddr,
                           uint8_t* data, size_t len) override {
        using namespace sensor::drivers;
        std::memset(data, 0, len);
        if (regAddr == Accelerometer::DEVID_REG && len >= 1U) {
            data[0] = Accelerometer::DEVID;   // ADXL345 = 0xE5
        }
        // Simulate ~1 g on Z axis (256 LSB × 0.004 g/LSB = 1.024 g)
        if (regAddr == Accelerometer::DATA_REG && len >= 6U) {
            // DATAZ1 = 0x01, DATAZ0 = 0x00 → 256 in little-endian
            data[4] = 0x00U;  // DATAZ0 (LSB)
            data[5] = 0x01U;  // DATAZ1 (MSB)
        }
        return sensor::Status::Ok;
    }
};

// ── Helper ────────────────────────────────────────────────────────────────

static void checkStatus(sensor::Status st, const char* msg) {
    if (st != sensor::Status::Ok) {
        std::printf("[ERROR] %s (status=%d)\n", msg, static_cast<int>(st));
    }
}

// ── Main ──────────────────────────────────────────────────────────────────

int main() {
    StubI2C i2c;
    StubSPI spi;

    // BME280 — temperature / pressure / humidity
    sensor::drivers::TemperatureSensor bme280{i2c, 0x76U};
    // BMP280 — pressure / temperature (no humidity)
    sensor::drivers::PressureSensor    bmp280{i2c, 0x77U};
    // ADXL345 — 3-axis accelerometer
    sensor::drivers::Accelerometer     adxl345{spi,
                                               sensor::drivers::Accelerometer::Range::G2,
                                               sensor::drivers::Accelerometer::DataRate::Hz100};

    // ── Initialise ────────────────────────────────────────────────────────
    checkStatus(bme280.init(),   "BME280 init");
    checkStatus(bmp280.init(),   "BMP280 init");
    checkStatus(adxl345.init(),  "ADXL345 init");

    std::printf("Devices initialised\n");
    std::printf("  BME280  id=0x%02X  name=%s\n",
                bme280.getDeviceId(), bme280.getName());
    std::printf("  BMP280  id=0x%02X  name=%s\n",
                bmp280.getDeviceId(), bmp280.getName());
    std::printf("  ADXL345 id=0x%02X  name=%s\n",
                adxl345.getDeviceId(), adxl345.getName());

    // ── Read BME280 ───────────────────────────────────────────────────────
    float temp{}, press{}, hum{};
    checkStatus(bme280.readAll(temp, press, hum), "BME280 readAll");
    std::printf("\nBME280  temp=%.2f °C  press=%.2f Pa  hum=%.2f %%RH\n",
                static_cast<double>(temp),
                static_cast<double>(press),
                static_cast<double>(hum));

    // ── Read BMP280 ───────────────────────────────────────────────────────
    float b_temp{}, b_press{};
    checkStatus(bmp280.readBoth(b_temp, b_press), "BMP280 readBoth");
    std::printf("BMP280  temp=%.2f °C  press=%.2f Pa\n",
                static_cast<double>(b_temp),
                static_cast<double>(b_press));

    // ── Read ADXL345 ──────────────────────────────────────────────────────
    float x{}, y{}, z{};
    checkStatus(adxl345.readAcceleration(x, y, z), "ADXL345 readAcceleration");
    std::printf("ADXL345 x=%.3f g  y=%.3f g  z=%.3f g\n",
                static_cast<double>(x),
                static_cast<double>(y),
                static_cast<double>(z));

    // ── Connection check ──────────────────────────────────────────────────
    std::printf("\nConnection status: BME280=%s  BMP280=%s  ADXL345=%s\n",
                bme280.isConnected()  ? "OK" : "FAIL",
                bmp280.isConnected()  ? "OK" : "FAIL",
                adxl345.isConnected() ? "OK" : "FAIL");

    return 0;
}
