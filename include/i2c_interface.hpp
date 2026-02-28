#pragma once

#include <cstddef>
#include <cstdint>

#include "sensor_base.hpp"

namespace sensor {

class II2C {
public:
    virtual ~II2C() = default;

    virtual Status writeReg(uint8_t devAddr, uint8_t regAddr,
                            const uint8_t* data, size_t len) = 0;
    virtual Status readReg(uint8_t devAddr, uint8_t regAddr,
                           uint8_t* data, size_t len) = 0;
};

} // namespace sensor
