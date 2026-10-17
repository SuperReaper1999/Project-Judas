#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Optional, owner-thread-only framed TCP transport. It has no world, entity,
// authentication or serialization policy. Numeric IPv4 addresses avoid blocking
// name resolution. Poll is the only operation which performs socket I/O.
class NetworkTransport {
public:
    struct Settings {
        uint32_t maxPeers = 8;
        uint32_t maxFrameBytes = 65536;
        uint32_t maxQueuedBytes = 262144;
        uint32_t bytesPerSecond = 131072;
        uint32_t connectTimeoutMs = 5000;
        uint32_t idleTimeoutMs = 10000;
    };
    struct Event {
        enum Type { Connected, Message, Disconnected, Error } type = Error;
        uint32_t peer = 0;
        std::string data;
    };
    struct Stats {
        uint64_t bytesSent = 0, bytesReceived = 0;
        uint64_t framesSent = 0, framesReceived = 0;
        uint64_t connections = 0, disconnections = 0;
        uint64_t dropped = 0, rejected = 0, backpressure = 0;
    };

    NetworkTransport();
    ~NetworkTransport();
    NetworkTransport(const NetworkTransport&) = delete;
    NetworkTransport& operator=(const NetworkTransport&) = delete;

    bool Listen(const std::string& numericIPv4, uint16_t port,
                const Settings&, std::string& error);
    bool Connect(const std::string& numericIPv4, uint16_t port,
                 const Settings&, std::string& error);
    // Supply finite, nondecreasing steady-clock seconds. The first Poll starts
    // timeout accounting. Each call replaces Events(); consume before next Poll.
    void Poll(double monotonicSeconds);
    // Queue a complete frame, never a partial application message. Replaceable
    // frames coalesce older replaceable frames only before any of their bytes
    // have been sent. Reliable/control frames are never discarded or reordered.
    bool Send(uint32_t peer, const std::string& data, bool replaceable,
              std::string& error);
    // Local closure appears in the next Poll's events. Stop discards events and
    // pending work, closes every descriptor and retains the last statistics.
    void Disconnect(uint32_t peer, const std::string& reason);
    void Stop();
    const std::vector<Event>& Events() const;
    const Stats& GetStats() const;
    size_t PendingBytes(uint32_t peer) const;
    uint16_t BoundPort() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
