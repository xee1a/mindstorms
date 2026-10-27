#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>
#include <stdexcept>

namespace nxt {

constexpr uint16_t LegoVendorId = 0x0694;
constexpr uint16_t NxtProductId = 0x0002;

class TimeoutError : public std::runtime_error {
public:
    TimeoutError() : std::runtime_error("The NXT did not respond in time") {}
};

/** The link is gone (cable pulled, brick switched off). */
class DisconnectedError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/** A blocking byte-packet link to one brick. Used from a single worker thread. */
class Transport {
public:
    virtual ~Transport() = default;
    virtual bool isDemo() const = 0;
    virtual QString label() const = 0;
    /** Throws DisconnectedError or std::runtime_error. */
    virtual void write(const QByteArray &packet) = 0;
    /** Throws TimeoutError, DisconnectedError or std::runtime_error. */
    virtual QByteArray read(int timeoutMs) = 0;
};

} // namespace nxt
