#include "NetworkTransport.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using RawSocket = SOCKET;
constexpr RawSocket BadSocket = INVALID_SOCKET;
void RawClose(RawSocket socket) { if (socket != BadSocket) closesocket(socket); }
void RawNonblocking(RawSocket socket) { u_long yes = 1; ioctlsocket(socket, FIONBIO, &yes); }
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
using RawSocket = int;
constexpr RawSocket BadSocket = -1;
void RawClose(RawSocket socket) { if (socket != BadSocket) close(socket); }
void RawNonblocking(RawSocket socket) { fcntl(socket, F_SETFL, fcntl(socket, F_GETFL) | O_NONBLOCK); }
#endif

namespace {
int checks = 0, failures = 0;
void Check(bool success, const char* text) {
    ++checks;
    failures += !success;
    std::printf("%s %s\n", success ? "PASS" : "FAIL", text);
}
using Events = std::vector<NetworkTransport::Event>;
void Append(Events& output, const NetworkTransport& transport) {
    output.insert(output.end(), transport.Events().begin(), transport.Events().end());
}
size_t Count(const Events& events, NetworkTransport::Event::Type type) {
    return static_cast<size_t>(std::count_if(events.begin(), events.end(),
        [type](const auto& event) { return event.type == type; }));
}
std::vector<std::string> Messages(const Events& events) {
    std::vector<std::string> output;
    for (const auto& event : events) if (event.type == NetworkTransport::Event::Message) output.push_back(event.data);
    return output;
}
void Tick(NetworkTransport& transport, double& now, Events& events, double step = .001) {
    now += step;
    transport.Poll(now);
    Append(events, transport);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
void Pump(NetworkTransport& server, NetworkTransport& client, double& now,
          Events& serverEvents, Events& clientEvents, int ticks = 40) {
    for (int i = 0; i < ticks; ++i) {
        now += .001;
        client.Poll(now); Append(clientEvents, client);
        server.Poll(now); Append(serverEvents, server);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
RawSocket RawConnect(uint16_t port) {
    RawSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == BadSocket) return socket;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        RawClose(socket); return BadSocket;
    }
    RawNonblocking(socket);
    return socket;
}
bool RawSend(RawSocket socket, const std::string& bytes) {
    size_t offset = 0;
    for (int i = 0; i < 100 && offset < bytes.size(); ++i) {
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const int count = static_cast<int>(send(socket, bytes.data() + offset,
            static_cast<int>(bytes.size() - offset), flags));
        if (count > 0) offset += static_cast<size_t>(count);
        else std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return offset == bytes.size();
}
std::string Frame(const std::string& data) {
    const uint32_t length = static_cast<uint32_t>(data.size());
    std::string frame(4, '\0');
    frame[0] = static_cast<char>(length >> 24); frame[1] = static_cast<char>(length >> 16);
    frame[2] = static_cast<char>(length >> 8); frame[3] = static_cast<char>(length);
    return frame + data;
}
void RawRead(RawSocket socket, std::string& output) {
    char bytes[1024];
    for (int i = 0; i < 64; ++i) {
        const int count = static_cast<int>(recv(socket, bytes, sizeof(bytes), 0));
        if (count <= 0) break;
        output.append(bytes, static_cast<size_t>(count));
    }
}
}

int main() {
    using Type = NetworkTransport::Event;
    NetworkTransport::Settings settings;
    std::string error;
    {
        NetworkTransport server, client;
        Check(server.Listen("127.0.0.1", 0, settings, error) && server.BoundPort() != 0,
              "numeric loopback listener chooses and publishes its actual ephemeral port");
        Check(!server.Listen("127.0.0.1", 0, settings, error) && !error.empty(),
              "active transport cannot silently replace a live listener");
        Check(client.Connect("127.0.0.1", server.BoundPort(), settings, error), "nonblocking loopback connect starts");
        double now = 100;
        Events a, b;
        Pump(server, client, now, a, b);
        Check(Count(a, Type::Connected) == 1 && Count(b, Type::Connected) == 1 && b.front().peer == 1,
              "both actual socket endpoints report one connection; client server handle is peer one");
        const uint32_t peer = a.empty() ? 0 : a.front().peer;
        const std::string binary("hello\0world", 11);
        Check(client.Send(1, binary, false, error) && client.Send(1, "", false, error) &&
              client.Send(1, "last", false, error), "binary, empty and ordinary reliable frames enqueue");
        Pump(server, client, now, a, b);
        Check(Messages(a) == std::vector<std::string>({binary, "", "last"}),
              "network endian framing preserves payload bytes and reliable FIFO boundaries");
        Check(server.Send(peer, "reply", false, error), "server addresses accepted connection identity");
        Pump(server, client, now, a, b);
        Check(Messages(b) == std::vector<std::string>({"reply"}) && client.PendingBytes(1) == 0,
              "bidirectional socket delivery drains counted queued bytes");
        Check(client.GetStats().bytesSent == binary.size() + 4 + 4 + 8 &&
              server.GetStats().framesReceived == 3 && server.GetStats().framesSent == 1,
              "wire byte and complete-frame counters describe actual I/O");
        Check(!client.Send(999, "stale", false, error) && !error.empty(), "unknown peer send fails diagnostically");
        client.Disconnect(1, "test leave");
        Pump(server, client, now, a, b);
        Check(Count(a, Type::Disconnected) == 1 && Count(b, Type::Disconnected) == 1,
              "local and remote disconnects each publish once without a stale connection");
        Check(!server.Send(peer, "stale", false, error), "disconnected generation cannot send to retired connection");
        const uint16_t port = server.BoundPort();
        server.Stop();
        Check(server.BoundPort() == 0 && server.Events().empty(), "Stop closes listener and clears transient observations");
        Check(server.Listen("127.0.0.1", port, settings, error) &&
              client.Connect("127.0.0.1", port, settings, error), "same transport can rebind and reconnect after Stop");
        a.clear(); b.clear(); Pump(server, client, now, a, b);
        Check(Count(a, Type::Connected) == 1 && a.front().peer > peer,
              "server connection identifiers never alias a previously retired connection across restarts");
        server.Poll(now - 1);
        Check(Count(server.Events(), Type::Error) == 1, "invalid clock regression is reported without corrupting live sockets");
        server.Poll(now);
        Check(server.Events().empty(), "each Poll replaces the previous event batch");
    }
    {
        NetworkTransport transport;
        Check(!transport.Connect("localhost", 12345, settings, error), "blocking hostname resolution is not accepted");
        Check(!transport.Connect("127.0.0.1", 0, settings, error), "zero connect port is rejected");
        auto invalid = settings; invalid.maxPeers = 0;
        Check(!transport.Listen("127.0.0.1", 0, invalid, error), "invalid capacity cannot create a transport");
        invalid = settings; invalid.maxFrameBytes = 0xffffffffu;
        Check(!transport.Listen("127.0.0.1", 0, invalid, error), "unbounded configuration rejects before opening sockets");
    }
    {
        NetworkTransport server;
        double now = 0;
        Events events;
        Check(server.Listen("127.0.0.1", 0, settings, error), "raw framing fixture listens");
        RawSocket raw = RawConnect(server.BoundPort());
        Check(raw != BadSocket, "independent raw TCP endpoint connects");
        Tick(server, now, events);
        Check(RawSend(raw, std::string("\0\0", 2)), "write split frame header first half");
        Tick(server, now, events);
        Check(Count(events, Type::Message) == 0, "partial header cannot become a message");
        Check(RawSend(raw, std::string("\0\x06", 2) + "ab"), "write remaining header and partial body");
        Tick(server, now, events);
        Check(Count(events, Type::Message) == 0, "partial body remains private and bounded");
        Check(RawSend(raw, "cdef"), "complete partial body");
        for (int i = 0; i < 10; ++i) Tick(server, now, events);
        Check(Messages(events) == std::vector<std::string>({"abcdef"}), "fragmented frame publishes only its complete exact payload");
        Check(RawSend(raw, std::string(4, static_cast<char>(0xff))), "independent endpoint sends hostile oversized length");
        for (int i = 0; i < 10; ++i) Tick(server, now, events);
        Check(Count(events, Type::Disconnected) == 1 && server.GetStats().rejected == 1 &&
              Count(events, Type::Message) == 1, "malformed length disconnects before allocation or phantom message");
        RawClose(raw);
        raw = RawConnect(server.BoundPort());
        events.clear(); Tick(server, now, events);
        RawSend(raw, Frame("unfinished").substr(0, 7)); Tick(server, now, events);
        RawClose(raw);
        for (int i = 0; i < 10; ++i) Tick(server, now, events);
        Check(Count(events, Type::Disconnected) == 1 && Count(events, Type::Message) == 0,
              "disconnect in mid-frame discards incomplete state without delivery");
    }
    {
        NetworkTransport server;
        auto bounded = settings;
        bounded.maxFrameBytes = 128; bounded.maxQueuedBytes = 260;
        bounded.bytesPerSecond = 8; bounded.idleTimeoutMs = 100000;
        Check(server.Listen("127.0.0.1", 0, bounded, error), "low-rate transport fixture listens");
        RawSocket raw = RawConnect(server.BoundPort());
        double now = 0;
        Events events;
        Tick(server, now, events);
        const uint32_t peer = events.empty() ? 0 : events.front().peer;
        Check(server.Send(peer, "old", true, error) && server.Send(peer, "new", true, error) &&
              server.PendingBytes(peer) == 7 && server.GetStats().dropped == 1,
              "entirely unsent replaceable snapshots coalesce");
        server.Stop(); RawClose(raw);
        Check(server.Listen("127.0.0.1", 0, bounded, error), "partial-send fixture starts cleanly");
        raw = RawConnect(server.BoundPort()); events.clear(); Tick(server, now, events);
        const uint32_t partialPeer = events.empty() ? 0 : events.front().peer;
        const std::string old(100, 'o');
        Check(server.Send(partialPeer, old, true, error), "old replaceable frame queues");
        Tick(server, now, events, 0);
        Check(server.PendingBytes(partialPeer) == 96 && server.GetStats().bytesSent == 8,
              "token budget bounds even the first partial frame including its header");
        Check(server.Send(partialPeer, "control", false, error) &&
              server.Send(partialPeer, "intermediate", true, error) &&
              server.Send(partialPeer, "newest", true, error), "queue controls and updated snapshot behind partially sent frame");
        std::string wire;
        for (int i = 0; i < 24; ++i) { RawRead(raw, wire); Tick(server, now, events, 1); }
        RawRead(raw, wire);
        Check(wire == Frame(old) + Frame("control") + Frame("newest"),
              "coalescing never edits partial wire frames or discards/reorders reliable control");
        Check(server.PendingBytes(partialPeer) == 0, "bounded low-rate sender eventually completes queued frames");
        Check(server.Send(partialPeer, old, false, error) && server.Send(partialPeer, old, false, error) &&
              !server.Send(partialPeer, old, false, error) && server.PendingBytes(partialPeer) == 208,
              "full reliable queue backpressures atomically rather than discarding control");
        Check(!server.Send(partialPeer, std::string(129, 'x'), true, error), "outgoing oversize frame is rejected before queue allocation");
        RawClose(raw);
    }
    {
        NetworkTransport server;
        auto bounded = settings;
        bounded.bytesPerSecond = 8; bounded.idleTimeoutMs = 100000;
        Check(server.Listen("127.0.0.1", 0, bounded, error), "inbound token fixture listens");
        RawSocket raw = RawConnect(server.BoundPort());
        double now = 0;
        Events events;
        Tick(server, now, events);
        const std::string payload(36, 'i');
        Check(RawSend(raw, Frame(payload)), "raw endpoint can burst beyond receiver processing budget");
        Tick(server, now, events, 0);
        Check(server.GetStats().bytesReceived == 8 && Count(events, Type::Message) == 0,
              "inbound partial headers and bodies share the same byte rate budget");
        Tick(server, now, events, 0);
        Check(server.GetStats().bytesReceived == 8, "repeated zero-time Poll cannot bypass receiver rate limiting");
        for (int i = 0; i < 8; ++i) Tick(server, now, events, 1);
        Check(Messages(events) == std::vector<std::string>({payload}) && server.GetStats().bytesReceived == 40,
              "token refill preserves and eventually completes a frame larger than the burst budget");
        RawClose(raw);
    }
    {
        NetworkTransport server;
        Check(server.Listen("127.0.0.1", 0, settings, error), "inbound message-count fixture listens");
        RawSocket raw = RawConnect(server.BoundPort());
        double now = 0;
        Events events;
        Tick(server, now, events);
        std::string burst;
        for (int i = 0; i < 80; ++i) burst += Frame(std::to_string(i));
        Check(RawSend(raw, burst), "independent endpoint writes many tiny frames in one burst");
        Tick(server, now, events);
        Check(Count(server.Events(), Type::Message) <= 32, "one peer cannot publish an unbounded callback flood per Poll");
        for (int i = 0; i < 5; ++i) Tick(server, now, events);
        const auto messages = Messages(events);
        bool fifo = messages.size() == 80;
        for (size_t i = 0; i < messages.size(); ++i) fifo = fifo && messages[i] == std::to_string(i);
        Check(fifo, "bounded message batches retain all reliable frames in FIFO order across Poll calls");
        RawClose(raw);
    }
    {
        NetworkTransport server;
        auto one = settings; one.maxPeers = 1; one.idleTimeoutMs = 100;
        Check(server.Listen("127.0.0.1", 0, one, error), "single-peer timeout fixture listens");
        RawSocket first = RawConnect(server.BoundPort());
        RawSocket second = RawConnect(server.BoundPort());
        double now = 10;
        Events events;
        Tick(server, now, events);
        Check(Count(events, Type::Connected) == 1 && server.GetStats().rejected == 1,
              "accepted-peer capacity rejects excess connections without growing retained state");
        Tick(server, now, events, .2);
        Check(Count(events, Type::Disconnected) == 1, "receive idle deadline closes a silent peer deterministically");
        RawClose(first); RawClose(second);
    }
    {
        NetworkTransport server;
        Check(server.Listen("127.0.0.1", 0, settings, error), "tiny-frame queue fixture listens");
        RawSocket raw = RawConnect(server.BoundPort());
        double now = 0;
        Events events;
        Tick(server, now, events);
        const uint32_t peer = events.empty() ? 0 : events.front().peer;
        bool queued = true;
        for (int i = 0; i < 1024; ++i) queued = server.Send(peer, "", false, error) && queued;
        Check(queued && !server.Send(peer, "", false, error) && server.PendingBytes(peer) == 4096,
              "empty reliable messages cannot evade the independent queued-frame capacity");
        Tick(server, now, events);
        Check(server.GetStats().framesSent <= 32 && server.PendingBytes(peer) != 0,
              "outbound socket calls stay bounded even for a queue of empty frames");
        RawClose(raw);
    }
    {
        NetworkTransport listener;
        Check(listener.Listen("127.0.0.1", 0, settings, error), "refused-connect fixture obtains an unused local port");
        const uint16_t port = listener.BoundPort();
        NetworkTransport conflicting;
        const bool conflict = !conflicting.Listen("127.0.0.1", port, settings, error);
        bool conflictExplained = error.find("address already in use") != std::string::npos;
#ifdef _WIN32
        // Exclusive Windows listeners can reject a second bind with WSAEACCES.
        conflictExplained |= error.find("permission denied") != std::string::npos;
#endif
        Check(conflict && conflictExplained &&
              error.find("127.0.0.1:" + std::to_string(port)) != std::string::npos,
              "real bind conflict names the endpoint and corrective action");
        listener.Stop();
        NetworkTransport client;
        const bool started = client.Connect("127.0.0.1", port, settings, error);
        double now = 0;
        Events events;
        if (started) for (int i = 0; i < 20; ++i) Tick(client, now, events, .5);
        Check((!started && !error.empty()) || Count(events, Type::Disconnected) == 1,
              "asynchronous connection refusal has an explicit terminal diagnostic");
        std::string diagnostic = started && !events.empty() ? events.back().data : error;
        Check(diagnostic.find("connection refused") != std::string::npos &&
              diagnostic.find("127.0.0.1:" + std::to_string(port)) != std::string::npos &&
              diagnostic.find("host is listening") != std::string::npos,
              "connection refusal identifies endpoint and host-listener check without raw-code guessing");
    }
    std::printf("SUMMARY checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
