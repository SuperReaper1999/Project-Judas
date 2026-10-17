#!/usr/bin/env python3
"""Bounded local TCP impairment relay for M99 evidence.

Delay/jitter preserve complete transport-frame order. Optional deterministic
message omission applies only to the named JSON message kind: this is explicit
application snapshot loss, not a claim to simulate TCP/IP packet loss. Never
drop arbitrary TCP bytes and pretend reliable transport should recover them.
"""
import argparse
import collections
import json
import math
from pathlib import Path
import random
import selectors
import signal
import socket
import struct
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--listen', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=28000)
    parser.add_argument('--upstream', default='127.0.0.1')
    parser.add_argument('--upstream-port', type=int, default=27999)
    parser.add_argument('--latency-ms', type=float, default=75)
    parser.add_argument('--jitter-ms', type=float, default=10)
    parser.add_argument('--drop-type', default='')
    parser.add_argument('--type-key', default='t')
    parser.add_argument('--drop-every', type=int, default=0)
    parser.add_argument('--seconds', type=float, default=0)
    parser.add_argument('--cut-after', type=float, default=0,
                        help='one real connection break after this many seconds from the first accept')
    parser.add_argument('--seed', type=int, default=99)
    parser.add_argument('--stats', type=Path)
    args = parser.parse_args()
    if (not all(math.isfinite(value) for value in [args.latency_ms, args.jitter_ms, args.seconds, args.cut_after]) or
            args.latency_ms < 0 or args.jitter_ms < 0 or args.drop_every < 0 or
            args.seconds < 0 or args.cut_after < 0 or not 0 <= args.port <= 65535 or
            not 1 <= args.upstream_port <= 65535):
        parser.error('invalid bounds')
    for address in [args.listen, args.upstream]:
        socket.inet_pton(socket.AF_INET, address)
    rng = random.Random(args.seed)
    selector = selectors.DefaultSelector()
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind((args.listen, args.port))
    listener.listen(8)
    listener.setblocking(False)
    selector.register(listener, selectors.EVENT_READ, None)
    endpoints = {}
    stats = dict(connections=0, frames=0, omitted=0, bytes=0, peakQueuedBytes=0,
                 violations=0, forcedDisconnects=0, seed=args.seed, directionalLatencyMs=args.latency_ms,
                 jitterMs=args.jitter_ms, omittedMessageKind=args.drop_type,
                 dropEvery=args.drop_every,
                 semantics='ordered TCP frames; optional application message omission')
    running = True
    cut_at = 0
    was_cut = False

    def stop(*_):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    def close_pair(endpoint):
        for item in [endpoint, endpoint['other']]:
            sock = item['socket']
            if sock in endpoints:
                selector.unregister(sock)
                endpoints.pop(sock)
                sock.close()

    def new_endpoint(sock):
        sock.setblocking(False)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        result = dict(socket=sock, incoming=bytearray(), outgoing=collections.deque(),
                      queued=0, last=0, candidate=0)
        endpoints[sock] = result
        selector.register(sock, selectors.EVENT_READ, result)
        return result

    started = time.monotonic()
    print(json.dumps(dict(ready=True, port=listener.getsockname()[1], **stats)), flush=True)
    try:
        while running and (not args.seconds or time.monotonic() - started < args.seconds):
            now = time.monotonic()
            if args.cut_after and cut_at and not was_cut and now >= cut_at:
                stats['forcedDisconnects'] += len(endpoints) // 2
                for endpoint in list(endpoints.values()):
                    if endpoint['socket'] in endpoints:
                        close_pair(endpoint)
                was_cut = True
            for endpoint in list(endpoints.values()):
                queue = endpoint['outgoing']
                events = selectors.EVENT_READ
                if queue and queue[0][0] <= now:
                    events |= selectors.EVENT_WRITE
                selector.modify(endpoint['socket'], events, endpoint)
            for key, events in selector.select(.002):
                endpoint = key.data
                if endpoint is None:
                    connection, _ = listener.accept()
                    if len(endpoints) >= 16:
                        connection.close()
                        stats['violations'] += 1
                        continue
                    try:
                        upstream = socket.create_connection(
                            (args.upstream, args.upstream_port), timeout=1)
                    except OSError:
                        connection.close()
                        continue
                    client = new_endpoint(connection)
                    server = new_endpoint(upstream)
                    client['other'], server['other'] = server, client
                    stats['connections'] += 1
                    if not cut_at:
                        cut_at = time.monotonic() + args.cut_after
                    continue
                if endpoint['socket'] not in endpoints:
                    continue
                try:
                    if events & selectors.EVENT_READ:
                        data = endpoint['socket'].recv(65536)
                        if not data:
                            close_pair(endpoint)
                            continue
                        endpoint['incoming'].extend(data)
                        incoming = endpoint['incoming']
                        while len(incoming) >= 4:
                            length, = struct.unpack_from('!I', incoming)
                            if length > 65536:
                                raise ValueError('frame exceeds M99 proxy 64KiB bound')
                            if len(incoming) < length + 4:
                                break
                            frame = bytes(incoming[:length + 4])
                            del incoming[:length + 4]
                            omit = False
                            if args.drop_type and args.drop_every:
                                try:
                                    value = json.loads(frame[4:])
                                except (ValueError, UnicodeDecodeError):
                                    value = None
                                if isinstance(value, dict) and value.get(args.type_key) == args.drop_type:
                                    endpoint['candidate'] += 1
                                    omit = endpoint['candidate'] % args.drop_every == 0
                            if omit:
                                stats['omitted'] += 1
                                continue
                            other = endpoint['other']
                            if other['queued'] + len(frame) > 1024 * 1024:
                                raise ValueError('delayed frame queue exceeds 1MiB per direction')
                            delay = max(0, args.latency_ms + rng.uniform(-args.jitter_ms, args.jitter_ms)) / 1000
                            deadline = max(other['last'], time.monotonic() + delay)
                            other['last'] = deadline
                            other['outgoing'].append([deadline, memoryview(frame)])
                            other['queued'] += len(frame)
                            stats['frames'] += 1
                            stats['peakQueuedBytes'] = max(stats['peakQueuedBytes'], other['queued'])
                    if events & selectors.EVENT_WRITE:
                        queue = endpoint['outgoing']
                        if queue and queue[0][0] <= time.monotonic():
                            sent = endpoint['socket'].send(queue[0][1])
                            stats['bytes'] += sent
                            endpoint['queued'] -= sent
                            if sent == len(queue[0][1]):
                                queue.popleft()
                            else:
                                queue[0][1] = queue[0][1][sent:]
                except BlockingIOError:
                    pass
                except (OSError, ValueError):
                    stats['violations'] += 1
                    close_pair(endpoint)
    finally:
        for endpoint in list(endpoints.values()):
            if endpoint['socket'] in endpoints:
                close_pair(endpoint)
        selector.unregister(listener)
        listener.close()
        selector.close()
        stats['elapsedSeconds'] = time.monotonic() - started
        if args.stats:
            args.stats.parent.mkdir(parents=True, exist_ok=True)
            args.stats.write_text(json.dumps(stats, indent=2) + '\n')
        print(json.dumps(stats), flush=True)


if __name__ == '__main__':
    main()
