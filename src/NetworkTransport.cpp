#include "NetworkTransport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket InvalidSocket = INVALID_SOCKET;
int LastError() { return WSAGetLastError(); }
bool WouldBlock(int error) { return error == WSAEWOULDBLOCK; }
bool Interrupted(int error) { return error == WSAEINTR; }
bool Connecting(int error) { return WouldBlock(error) || error == WSAEINPROGRESS; }
void Close(Socket socket) { if (socket != InvalidSocket) closesocket(socket); }
bool Nonblocking(Socket socket) { u_long yes = 1; return ioctlsocket(socket, FIONBIO, &yes) == 0; }
using SockLength = int;
#else
using Socket = int;
constexpr Socket InvalidSocket = -1;
int LastError() { return errno; }
bool WouldBlock(int error) { return error == EWOULDBLOCK || error == EAGAIN; }
bool Interrupted(int error) { return error == EINTR; }
bool Connecting(int error) { return error == EINPROGRESS || WouldBlock(error); }
void Close(Socket socket) { if (socket != InvalidSocket) close(socket); }
bool Nonblocking(Socket socket) {
    const int flags = fcntl(socket, F_GETFL, 0);
    return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0 &&
           fcntl(socket, F_SETFD, FD_CLOEXEC) == 0;
}
using SockLength = socklen_t;
#endif
std::string Diagnostic(const char* operation, int error) {
    return std::string(operation) + " (socket error " + std::to_string(error) + ")";
}
bool Address(const std::string& text, uint16_t port, sockaddr_in& address, std::string& error) {
    address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (text.size() > 15 || inet_pton(AF_INET, text.c_str(), &address.sin_addr) != 1) {
        error = "network address must be a numeric IPv4 address";
        return false;
    }
    return true;
}
bool ValidSettings(const NetworkTransport::Settings& settings, std::string& error) {
    // Hard ceilings bound aggregate retention even when project configuration is
    // untrusted. A transport is deliberately a small-session primitive.
    if (!settings.maxPeers || settings.maxPeers > 64 || !settings.maxFrameBytes ||
        settings.maxFrameBytes > 1048576 || settings.maxQueuedBytes < settings.maxFrameBytes + 4 ||
        settings.maxQueuedBytes > 8388608 || !settings.bytesPerSecond ||
        settings.bytesPerSecond > 16777216 || !settings.connectTimeoutMs ||
        settings.connectTimeoutMs > 300000 || !settings.idleTimeoutMs ||
        settings.idleTimeoutMs > 3600000) {
        error = "invalid bounded network transport settings";
        return false;
    }
    return true;
}
void ConfigurePeer(Socket socket) {
    const int yes = 1;
    setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&yes), sizeof(yes));
#ifdef SO_NOSIGPIPE
    setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char*>(&yes), sizeof(yes));
#endif
}
constexpr size_t MaxIoBytesPerPeer = 65536;
constexpr size_t MaxFramesPerPeer = 32;
constexpr size_t MaxQueuedFramesPerPeer = 1024;
constexpr size_t MaxEventsPerPoll = 256;
constexpr size_t MaxAcceptsPerPoll = 16;
}

struct NetworkTransport::Impl {
    struct Frame {
        std::string bytes;
        size_t offset = 0;
        bool replaceable = false;
    };
    struct Peer {
        Socket socket = InvalidSocket;
        bool connecting = false;
        double created = 0, received = 0;
        double sendTokens = 0, receiveTokens = 0;
        std::array<unsigned char, 4> header{};
        size_t headerBytes = 0;
        uint32_t expected = 0;
        std::string incoming;
        std::deque<Frame> outgoing;
        size_t pending = 0;
    };
    Settings settings;
    Stats stats;
    Socket listener = InvalidSocket;
    std::map<uint32_t, Peer> peers;
    std::vector<Event> events, pendingEvents;
    uint32_t nextPeer = 1, lastPollFirst = 0;
    uint16_t port = 0;
    bool hasTime = false;
    double now = 0;
#ifdef _WIN32
    bool winsock = false;
#endif

    bool Startup(std::string& error) {
#ifdef _WIN32
        if (!winsock) {
            WSADATA data{};
            const int code = WSAStartup(MAKEWORD(2, 2), &data);
            if (code) { error = Diagnostic("WSAStartup failed", code); return false; }
            winsock = true;
        }
#else
        (void)error;
#endif
        return true;
    }
    void Stop() {
        Close(listener);
        listener = InvalidSocket;
        for (auto& item : peers) Close(item.second.socket);
        peers.clear();
        events.clear();
        pendingEvents.clear();
        port = 0;
        hasTime = false;
        now = 0;
#ifdef _WIN32
        if (winsock) { WSACleanup(); winsock = false; }
#endif
    }
    void EventNow(Event::Type type, uint32_t peer, std::string data) {
        if (events.size() < MaxEventsPerPoll) events.push_back({type, peer, std::move(data)});
        else ++stats.dropped;
    }
    void Remove(uint32_t id, const std::string& reason, bool immediate) {
        const auto found = peers.find(id);
        if (found == peers.end()) return;
        Close(found->second.socket);
        peers.erase(found);
        ++stats.disconnections;
        if (immediate) EventNow(Event::Disconnected, id, reason);
        else if (pendingEvents.size() < MaxEventsPerPoll)
            pendingEvents.push_back({Event::Disconnected, id, reason.substr(0, 512)});
        else ++stats.dropped;
    }
    Peer NewPeer(Socket socket, bool connecting) const {
        Peer peer;
        peer.socket = socket;
        peer.connecting = connecting;
        peer.created = peer.received = now;
        peer.sendTokens = peer.receiveTokens = settings.bytesPerSecond;
        return peer;
    }
};

NetworkTransport::NetworkTransport() : m_impl(new Impl) {}
NetworkTransport::~NetworkTransport() { m_impl->Stop(); }

bool NetworkTransport::Listen(const std::string& numericIPv4, uint16_t port,
                              const Settings& settings, std::string& error) {
    error.clear();
    sockaddr_in address{};
    if (!ValidSettings(settings, error) || !m_impl->Startup(error) ||
        !Address(numericIPv4, port, address, error)) return false;
    if (m_impl->listener != InvalidSocket || !m_impl->peers.empty()) {
        error = "network transport is already active"; return false;
    }
    Socket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == InvalidSocket) { error = Diagnostic("listen socket failed", LastError()); return false; }
    const int yes = 1;
#ifdef _WIN32
    setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
    if (!Nonblocking(socket) || bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(socket, static_cast<int>(settings.maxPeers)) != 0) {
        error = Diagnostic("listen/bind failed", LastError()); Close(socket); return false;
    }
    SockLength length = sizeof(address);
    if (getsockname(socket, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        error = Diagnostic("listen address failed", LastError()); Close(socket); return false;
    }
    m_impl->settings = settings;
    m_impl->stats = {};
    m_impl->listener = socket;
    m_impl->port = ntohs(address.sin_port);
    return true;
}

bool NetworkTransport::Connect(const std::string& numericIPv4, uint16_t port,
                               const Settings& settings, std::string& error) {
    error.clear();
    sockaddr_in address{};
    if (!port) { error = "connect port must be nonzero"; return false; }
    if (!ValidSettings(settings, error) || !m_impl->Startup(error) ||
        !Address(numericIPv4, port, address, error)) return false;
    if (m_impl->listener != InvalidSocket || !m_impl->peers.empty()) {
        error = "network transport is already active"; return false;
    }
    Socket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == InvalidSocket) { error = Diagnostic("connect socket failed", LastError()); return false; }
    if (!Nonblocking(socket)) {
        error = Diagnostic("nonblocking connect setup failed", LastError()); Close(socket); return false;
    }
    ConfigurePeer(socket);
    const int result = connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (result != 0 && !Connecting(LastError())) {
        error = Diagnostic("connect failed", LastError()); Close(socket); return false;
    }
    m_impl->settings = settings;
    m_impl->stats = {};
    m_impl->peers.emplace(1, m_impl->NewPeer(socket, result != 0));
    if (result == 0) {
        ++m_impl->stats.connections;
        m_impl->pendingEvents.push_back({Event::Connected, 1, {}});
    }
    return true;
}

void NetworkTransport::Poll(double monotonicSeconds) {
    Impl& impl = *m_impl;
    impl.events.clear();
    impl.events.swap(impl.pendingEvents);
    impl.pendingEvents.clear();
    if (!std::isfinite(monotonicSeconds) || (impl.hasTime && monotonicSeconds < impl.now)) {
        impl.EventNow(Event::Error, 0, "network Poll requires finite nondecreasing monotonic time");
        return;
    }
    const double elapsed = impl.hasTime ? monotonicSeconds - impl.now : 0;
    if (!impl.hasTime) {
        for (auto& item : impl.peers) item.second.created = item.second.received = monotonicSeconds;
    }
    impl.hasTime = true;
    impl.now = monotonicSeconds;

    if (impl.listener != InvalidSocket) {
        for (size_t count = 0; count < MaxAcceptsPerPoll; ++count) {
            Socket socket = accept(impl.listener, nullptr, nullptr);
            if (socket == InvalidSocket) {
                const int code = LastError();
                if (!WouldBlock(code)) impl.EventNow(Event::Error, 0, Diagnostic("accept failed", code));
                break;
            }
            if (impl.peers.size() >= impl.settings.maxPeers || impl.nextPeer == 0) {
                ++impl.stats.rejected; Close(socket); continue;
            }
            if (!Nonblocking(socket)) {
                impl.EventNow(Event::Error, 0, Diagnostic("accepted socket setup failed", LastError()));
                Close(socket); continue;
            }
            ConfigurePeer(socket);
            const uint32_t id = impl.nextPeer++;
            impl.peers.emplace(id, impl.NewPeer(socket, false));
            ++impl.stats.connections;
            impl.EventNow(Event::Connected, id, {});
        }
    }

    size_t messageBytes = 0;
    const size_t maxMessageBytes = std::max<size_t>(impl.settings.maxFrameBytes, 1048576);
    // Rotate the first serviced peer. A busy low-ID peer must not permanently
    // starve later peers when the shared event/byte batch reaches its bound.
    std::vector<uint32_t> ids;
    ids.reserve(impl.peers.size());
    for (const auto& item : impl.peers) ids.push_back(item.first);
    const auto first = std::upper_bound(ids.begin(), ids.end(), impl.lastPollFirst);
    if (first != ids.end()) std::rotate(ids.begin(), first, ids.end());
    if (!ids.empty()) impl.lastPollFirst = ids.front();
    for (uint32_t id : ids) {
        const auto found = impl.peers.find(id);
        if (found == impl.peers.end()) continue;
        Impl::Peer& peer = found->second;
        peer.sendTokens = std::min<double>(impl.settings.bytesPerSecond,
            peer.sendTokens + elapsed * impl.settings.bytesPerSecond);
        peer.receiveTokens = std::min<double>(impl.settings.bytesPerSecond,
            peer.receiveTokens + elapsed * impl.settings.bytesPerSecond);
        if (peer.connecting) {
            fd_set writable, failed;
            FD_ZERO(&writable); FD_ZERO(&failed);
#ifndef _WIN32
            if (peer.socket >= FD_SETSIZE) {
                impl.Remove(id, "connect descriptor exceeds select capacity", true); continue;
            }
#endif
            FD_SET(peer.socket, &writable); FD_SET(peer.socket, &failed);
            timeval immediate{};
            const int ready = select(static_cast<int>(peer.socket + 1), nullptr, &writable, &failed, &immediate);
            if (ready < 0) {
                const int code = LastError();
                if (!Interrupted(code)) impl.Remove(id, Diagnostic("connect poll failed", code), true);
                continue;
            }
            if (ready > 0) {
                int code = 0;
                SockLength length = sizeof(code);
                if (getsockopt(peer.socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&code), &length) != 0)
                    code = LastError();
                if (code) { impl.Remove(id, Diagnostic("connect failed", code), true); continue; }
                peer.connecting = false;
                peer.received = impl.now;
                ++impl.stats.connections;
                impl.EventNow(Event::Connected, id, {});
            } else if ((impl.now - peer.created) * 1000 >= impl.settings.connectTimeoutMs) {
                impl.Remove(id, "connect timed out", true); continue;
            } else continue;
        }
        if ((impl.now - peer.received) * 1000 >= impl.settings.idleTimeoutMs) {
            impl.Remove(id, "receive idle timeout", true); continue;
        }

        bool closed = false;
        size_t sentThisPoll = 0, sendCalls = 0;
        while (!peer.outgoing.empty() && sentThisPoll < MaxIoBytesPerPeer &&
               sendCalls < MaxFramesPerPeer && peer.sendTokens >= 1) {
            auto& frame = peer.outgoing.front();
            const size_t count = std::min({frame.bytes.size() - frame.offset,
                MaxIoBytesPerPeer - sentThisPoll, static_cast<size_t>(peer.sendTokens)});
#ifdef MSG_NOSIGNAL
            constexpr int flags = MSG_NOSIGNAL;
#else
            constexpr int flags = 0;
#endif
            const int sent = static_cast<int>(send(peer.socket, frame.bytes.data() + frame.offset,
                                                   static_cast<int>(count), flags));
            ++sendCalls;
            if (sent < 0) {
                const int code = LastError();
                if (!WouldBlock(code) && !Interrupted(code)) { impl.Remove(id, Diagnostic("send failed", code), true); closed = true; }
                else ++impl.stats.backpressure;
                break;
            }
            if (sent == 0) break;
            frame.offset += static_cast<size_t>(sent);
            peer.pending -= static_cast<size_t>(sent);
            peer.sendTokens -= sent;
            sentThisPoll += static_cast<size_t>(sent);
            impl.stats.bytesSent += static_cast<uint64_t>(sent);
            if (frame.offset == frame.bytes.size()) { peer.outgoing.pop_front(); ++impl.stats.framesSent; }
        }
        if (closed) continue;

        size_t receivedThisPoll = 0, framesThisPoll = 0;
        std::array<char, 8192> buffer{};
        while (receivedThisPoll < MaxIoBytesPerPeer && framesThisPoll < MaxFramesPerPeer &&
               impl.events.size() < MaxEventsPerPoll - impl.settings.maxPeers - MaxAcceptsPerPoll &&
               peer.receiveTokens >= 1) {
            const size_t needed = peer.headerBytes < 4 ? 4 - peer.headerBytes : peer.expected - peer.incoming.size();
            // Reserve room in the bounded event batch before consuming a body.
            if (peer.headerBytes == 4 && messageBytes + peer.expected > maxMessageBytes) break;
            const size_t count = std::min({needed, buffer.size(), MaxIoBytesPerPeer - receivedThisPoll,
                                           static_cast<size_t>(peer.receiveTokens)});
            const int received = static_cast<int>(recv(peer.socket, buffer.data(), static_cast<int>(count), 0));
            if (received <= 0) {
                const int code = received < 0 ? LastError() : 0;
                if (received == 0 || (!WouldBlock(code) && !Interrupted(code))) {
                    impl.Remove(id, received == 0 ? "remote closed connection" : Diagnostic("receive failed", code), true);
                    closed = true;
                }
                break;
            }
            peer.received = impl.now;
            peer.receiveTokens -= received;
            receivedThisPoll += static_cast<size_t>(received);
            impl.stats.bytesReceived += static_cast<uint64_t>(received);
            if (peer.headerBytes < 4) {
                std::memcpy(peer.header.data() + peer.headerBytes, buffer.data(), static_cast<size_t>(received));
                peer.headerBytes += static_cast<size_t>(received);
                if (peer.headerBytes < 4) continue;
                peer.expected = (uint32_t(peer.header[0]) << 24) | (uint32_t(peer.header[1]) << 16) |
                                (uint32_t(peer.header[2]) << 8) | uint32_t(peer.header[3]);
                if (peer.expected > impl.settings.maxFrameBytes) {
                    ++impl.stats.rejected;
                    impl.Remove(id, "frame length exceeds transport limit", true); closed = true; break;
                }
                peer.incoming.clear();
                peer.incoming.reserve(peer.expected);
            } else peer.incoming.append(buffer.data(), static_cast<size_t>(received));
            if (peer.headerBytes == 4 && peer.incoming.size() == peer.expected) {
                messageBytes += peer.expected;
                impl.EventNow(Event::Message, id, std::move(peer.incoming));
                peer.incoming = {};
                peer.headerBytes = 0;
                peer.expected = 0;
                ++framesThisPoll;
                ++impl.stats.framesReceived;
            }
        }
        (void)closed;
    }
}

bool NetworkTransport::Send(uint32_t id, const std::string& data, bool replaceable, std::string& error) {
    error.clear();
    Impl& impl = *m_impl;
    const auto found = impl.peers.find(id);
    if (found == impl.peers.end()) { error = "network peer is not connected"; ++impl.stats.rejected; return false; }
    if (data.size() > impl.settings.maxFrameBytes) {
        error = "outgoing frame exceeds transport limit"; ++impl.stats.rejected; return false;
    }
    auto& peer = found->second;
    const size_t frameBytes = data.size() + 4;
    size_t replaceBytes = 0, replaceFrames = 0;
    if (replaceable) for (const auto& frame : peer.outgoing)
        if (frame.replaceable && frame.offset == 0) { replaceBytes += frame.bytes.size(); ++replaceFrames; }
    if (peer.pending - replaceBytes + frameBytes > impl.settings.maxQueuedBytes ||
        peer.outgoing.size() - replaceFrames >= MaxQueuedFramesPerPeer) {
        error = "outgoing network queue is full"; ++impl.stats.backpressure; return false;
    }
    if (replaceable) for (auto item = peer.outgoing.begin(); item != peer.outgoing.end();) {
        if (item->replaceable && item->offset == 0) {
            peer.pending -= item->bytes.size(); item = peer.outgoing.erase(item); ++impl.stats.dropped;
        } else ++item;
    }
    Impl::Frame frame;
    frame.replaceable = replaceable;
    frame.bytes.resize(frameBytes);
    const uint32_t length = static_cast<uint32_t>(data.size());
    frame.bytes[0] = static_cast<char>(length >> 24);
    frame.bytes[1] = static_cast<char>(length >> 16);
    frame.bytes[2] = static_cast<char>(length >> 8);
    frame.bytes[3] = static_cast<char>(length);
    std::memcpy(frame.bytes.data() + 4, data.data(), data.size());
    peer.pending += frameBytes;
    peer.outgoing.push_back(std::move(frame));
    return true;
}

void NetworkTransport::Disconnect(uint32_t peer, const std::string& reason) { m_impl->Remove(peer, reason, false); }
void NetworkTransport::Stop() { m_impl->Stop(); }
const std::vector<NetworkTransport::Event>& NetworkTransport::Events() const { return m_impl->events; }
const NetworkTransport::Stats& NetworkTransport::GetStats() const { return m_impl->stats; }
size_t NetworkTransport::PendingBytes(uint32_t id) const {
    const auto found = m_impl->peers.find(id);
    return found == m_impl->peers.end() ? 0 : found->second.pending;
}
uint16_t NetworkTransport::BoundPort() const { return m_impl->port; }
