#pragma once

#include <cstddef>
#include <cstdint>

#include "sensor_base.hpp"

namespace sensor {

class ISPI {
public:
    virtual ~ISPI() = default;

    virtual Status writeReg(uint8_t regAddr, const uint8_t* data, size_t len) = 0;
    virtual Status readReg(uint8_t regAddr, uint8_t* data, size_t len) = 0;
};

} // namespace sensor
