# M99 transport-focused evidence

These are native Linux loopback checks of `NetworkTransport`, not a substitute
for the independent-process application, latency/loss, editor or export checks in
the final [report](REPORT.md). M98/M99 human acceptance is pending.

## Executed standalone check

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Isrc \
  src/NetworkTransport.cpp tests/NetworkTransportTests.cpp \
  -o /tmp/judas-m99-network-transport-tests
/tmp/judas-m99-network-transport-tests
```

Result: **59 checks, zero failures**. Compilation produced no warnings with those
flags. Actual IPv4 TCP sockets were used; the test does not fake a transport or
call a local message queue in place of network I/O.

The first earlier, 56-check run was executed within the network-restricted tool
sandbox. Listening and connecting failed, producing 40 downstream failures. The
unchanged executable passed all 56 checks with socket access outside that sandbox.
Subsequent source review added independent empty-frame queue and send-call bounds;
the final 59-check executable also passed outside the sandbox. The sandbox run is
an environment failure, not claimed as an engine regression or hidden as a pass.

Tested behaviour:

- Real connect/accept in both directions, binary/empty messages and exact FIFO framing.
- Partial header, partial body, hostile length before allocation, mid-frame closure.
- Local/remote disconnect, async refusal, idle timeout, Stop/rebind, nonreused server IDs.
- Atomic queue backpressure and entirely unsent snapshot coalescing.
- Eight-byte-per-second partial sends: a partly transmitted snapshot and reliable
  control survive later snapshot replacement with their exact original wire bytes.
- Inbound token saturation, frames larger than the token burst, repeated zero-time
  polls and eventual exact completion.
- Tiny-message ingress floods and a 1,024-empty-frame egress queue; callback and
  syscall work remain bounded independently of byte count.
- Invalid settings, numeric-address-only enforcement and regressing clock diagnostics.

## Exact transport inputs for that run

```text
2ec74e7801c1b0e3f3cefc969b93be07f84292bef1b036d03986b3ddea758fb5  src/NetworkTransport.h
329e587b5d250537d4cc93d4a2057553e7c2ac7eeeff97e250ffe7b7565d5eaf  src/NetworkTransport.cpp
b82548ddcd2bb5f6fae99c79f5c3d32cb03cd87e7374bb79399aefecfbc4fe96  tests/NetworkTransportTests.cpp
42c76ce0c0ae700e494910565455709f268a0e5786536a8b9b78438d4b0d30c8  standalone test executable
```

The executable is not committed. Final combined-source/binary fingerprints and
the CMake-integrated run are recorded separately by the milestone delivery tools.
No Windows or Android result is inferred from this Linux check.
