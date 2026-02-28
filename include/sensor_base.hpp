#pragma once

#include <cstdint>

namespace sensor {

enum class Status : uint8_t {
    Ok = 0,
    Error,
    NotFound,
    NotInitialized,
    InvalidArg,
    Timeout,
};

class ISensor {
public:
    ISensor() = default;
    virtual ~ISensor() = default;

    ISensor(const ISensor&) = delete;
    ISensor& operator=(const ISensor&) = delete;
    ISensor(ISensor&&) = default;
    ISensor& operator=(ISensor&&) = default;

    virtual Status init() = 0;
    virtual Status reset() = 0;
    virtual bool isConnected() = 0;
    virtual uint8_t getDeviceId() = 0;
    virtual const char* getName() = 0;
};

} // namespace sensor
