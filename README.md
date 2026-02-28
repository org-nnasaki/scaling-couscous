# EmbeddedSensorLib

C++17 embedded sensor driver library for BME280 (temperature/humidity/pressure),
BMP280 (pressure/temperature), and ADXL345 (3-axis accelerometer).  
All hardware communication is abstracted behind pure-virtual HAL interfaces so
drivers can be compiled and tested without real hardware.

## Project Structure

```
EmbeddedSensorLib/
├── CMakeLists.txt
├── include/
│   ├── sensor_base.hpp          # ISensor abstract base + Status enum
│   ├── i2c_interface.hpp        # II2C HAL interface
│   ├── spi_interface.hpp        # ISPI HAL interface
│   └── drivers/
│       ├── temperature_sensor.hpp  # BME280 driver
│       ├── pressure_sensor.hpp     # BMP280 driver
│       └── accelerometer.hpp       # ADXL345 driver
├── src/drivers/                 # Driver implementations
├── tests/                       # GoogleTest unit tests
├── examples/
│   └── main.cpp                 # Usage example
└── docs/
    ├── api_reference.md
    └── specs/                   # Driver specification Markdowns (source of truth)
```

## Build

### Prerequisites

- CMake ≥ 3.15
- C++17-capable compiler (GCC ≥ 7, Clang ≥ 5)
- Internet access for GoogleTest (fetched automatically via `FetchContent`)

### Configure and build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Build without tests:

```bash
cmake -B build -DBUILD_TESTS=OFF
cmake --build build
```

## Run Tests

```bash
cd build && ctest --output-on-failure
```

Run a single test binary:

```bash
./build/test_temperature
./build/test_accelerometer
./build/test_pressure
```

Run a specific test case:

```bash
./build/test_temperature --gtest_filter='TemperatureSensorTest.ReadTemperature_Success_ReturnsReasonableValue'
```

## Usage

Implement the HAL interfaces for your platform and pass them to the driver constructors.

```cpp
#include "drivers/temperature_sensor.hpp"
#include "drivers/pressure_sensor.hpp"
#include "drivers/accelerometer.hpp"

// Provide platform I2C / SPI implementations
class MyI2C : public sensor::II2C { ... };
class MySPI : public sensor::ISPI { ... };

int main() {
    MyI2C i2c;
    MySPI spi;

    sensor::drivers::TemperatureSensor bme280{i2c};
    sensor::drivers::PressureSensor    bmp280{i2c, 0x77};  // SDO=VDDIO
    sensor::drivers::Accelerometer     adxl345{spi};

    if (bme280.init() != sensor::Status::Ok) { /* handle error */ }
    if (bmp280.init() != sensor::Status::Ok) { /* handle error */ }
    if (adxl345.init() != sensor::Status::Ok) { /* handle error */ }

    float temp{}, press{}, hum{};
    bme280.readAll(temp, press, hum);

    float pressure{}, temperature{};
    bmp280.readBoth(temperature, pressure);

    float x{}, y{}, z{};
    adxl345.readAcceleration(x, y, z);
}
```

## Architecture

```
Application
    │
    ▼
sensor::drivers::  ← concrete drivers (sensor_base.hpp ISensor)
    │                 BME280 / BMP280 use II2C
    │                 ADXL345         uses ISPI
    ▼
sensor::II2C / ISPI  ← pure-virtual HAL (platform implements; tests mock)
```

All sensor operations return `sensor::Status`.  Always check the return value.

## Driver → Specification Mapping

| Driver | Spec file | Bus |
|---|---|---|
| `TemperatureSensor` | `docs/specs/bme280_driver_spec.md` | I2C |
| `PressureSensor` | `docs/specs/bmp280_driver_spec.md` | I2C |
| `Accelerometer` | `docs/specs/adxl345_driver_spec.md` | SPI |
