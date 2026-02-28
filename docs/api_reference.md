# API Reference

## `sensor::Status`

```cpp
enum class Status : uint8_t {
    Ok,
    Error,
    NotFound,
    NotInitialized,
    InvalidArg,
    Timeout,
};
```

Returned by every sensor operation.

---

## `sensor::ISensor`

Abstract base class for all sensor drivers.

```cpp
class ISensor {
public:
    virtual Status      init()        = 0;
    virtual Status      reset()       = 0;
    virtual bool        isConnected() = 0;
    virtual uint8_t     getDeviceId() = 0;
    virtual const char* getName()     = 0;
};
```

Non-copyable, movable.

---

## `sensor::II2C`

Pure-virtual I2C HAL interface.

```cpp
class II2C {
public:
    virtual Status writeReg(uint8_t devAddr, uint8_t regAddr,
                            const uint8_t* data, size_t len) = 0;
    virtual Status readReg (uint8_t devAddr, uint8_t regAddr,
                            uint8_t* data,  size_t len) = 0;
};
```

---

## `sensor::ISPI`

Pure-virtual SPI HAL interface.

```cpp
class ISPI {
public:
    virtual Status writeReg(uint8_t regAddr, const uint8_t* data, size_t len) = 0;
    virtual Status readReg (uint8_t regAddr, uint8_t* data,       size_t len) = 0;
};
```

---

## `sensor::drivers::TemperatureSensor` (BME280)

**Header:** `include/drivers/temperature_sensor.hpp`  
**Bus:** I2C (default address `0x76`, alternate `0x77`)  
**Chip ID:** `0x60`

### Constants

| Constant | Value | Description |
|---|---|---|
| `DEFAULT_I2C_ADDR` | `0x76` | Default I2C address (SDO=GND) |
| `CHIP_ID` | `0x60` | BME280 device identifier |
| `RESET_VALUE` | `0xB6` | Soft-reset magic byte |

### Constructor

```cpp
explicit TemperatureSensor(II2C& bus, uint8_t addr = DEFAULT_I2C_ADDR);
```

### `init()`

```cpp
Status init();
```

1. Reads chip ID from `0xD0` and verifies `== 0x60`.
2. Issues soft reset via `0xE0 ← 0xB6`.
3. Reads factory calibration data (26 bytes from `0x88`, 7 bytes from `0xE1`).
4. Configures humidity oversampling ×1 (`0xF2`), no IIR filter (`0xF5`),
   temperature/pressure oversampling ×1, Normal mode (`0xF4 ← 0x27`).

Returns `Status::NotFound` if the chip ID does not match.

### `readTemperature(float& temperature)`

Reads 3 bytes from `0xFA–0xFC`, applies the BME280 integer compensation
formula, and returns the result in **°C**.

### `readPressure(float& pressure)`

Reads 6 bytes from `0xF7–0xFC` (pressure + temperature), applies compensation,
and returns the result in **Pa** (Q24.8 → divide by 256).

### `readHumidity(float& humidity)`

Reads 8 bytes from `0xF7–0xFE`, applies compensation, and returns the result
in **%RH** (Q22.10 → divide by 1024).

### `readAll(float& temperature, float& pressure, float& humidity)`

Single burst read of all 8 data bytes; fills all three output parameters.

### `calibration()`

```cpp
const CalibrationData& calibration() const;
```

Returns the factory calibration coefficients read during `init()`.

---

## `sensor::drivers::PressureSensor` (BMP280)

**Header:** `include/drivers/pressure_sensor.hpp`  
**Bus:** I2C (default address `0x76`, alternate `0x77`)  
**Chip ID:** `0x58`

BMP280 shares the I2C protocol and register map with BME280 but has **no
humidity support** (`ctrl_hum`, humidity data/calibration registers absent).

### Constructor

```cpp
explicit PressureSensor(II2C& bus, uint8_t addr = DEFAULT_I2C_ADDR);
```

### `init()`

Same sequence as `TemperatureSensor::init()` minus the humidity calibration
and `ctrl_hum` register write.  Verifies chip ID `== 0x58`.

### `readTemperature(float& temperature)`

3-byte read from `0xFA–0xFC`; result in **°C**.

### `readPressure(float& pressure)`

6-byte read from `0xF7–0xFC`; result in **Pa**.

### `readBoth(float& temperature, float& pressure)`

Single burst read; fills both output parameters.

### `calibration()`

```cpp
const CalibrationData& calibration() const;
```

---

## `sensor::drivers::Accelerometer` (ADXL345)

**Header:** `include/drivers/accelerometer.hpp`  
**Bus:** SPI (4-wire, Mode 11)  
**Device ID:** `0xE5`

### Enums

```cpp
enum class Range : uint8_t { G2 = 0x00, G4 = 0x01, G8 = 0x02, G16 = 0x03 };
enum class DataRate : uint8_t { Hz12_5=0x06, Hz25=0x07, Hz50=0x08,
                                Hz100=0x09, Hz200=0x0A, Hz400=0x0B,
                                Hz800=0x0C, Hz1600=0x0D, Hz3200=0x0E };
```

### Constructor

```cpp
explicit Accelerometer(ISPI& bus,
                       Range range   = Range::G2,
                       DataRate rate = DataRate::Hz100);
```

### `init()`

1. Reads device ID from `0x00` and verifies `== 0xE5`.
2. Writes `DATA_FORMAT` (`0x31`): FULL_RES mode + selected range.
3. Writes `BW_RATE` (`0x2C`): selected data rate.
4. Disables interrupts (`0x2E ← 0x00`).
5. Disables FIFO (`0x38 ← 0x00`).
6. Sets `POWER_CTL` (`0x2D`) Measure bit to start continuous measurement.

### `readAcceleration(float& x, float& y, float& z)`

Multi-byte read of `DATAX0–DATAZ1` (6 bytes, auto-increment).  Each axis is a
signed 16-bit two's-complement little-endian value.

**Scale (FULL_RES mode):** 4 mg/LSB = `0.004 g/LSB` for all ranges.

Returns x, y, z in **g**.

### `setRange(Range range)`

Updates `DATA_FORMAT` register at runtime.  Can be called after `init()`.
