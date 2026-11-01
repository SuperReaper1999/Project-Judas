# Alpha v1.2.0 preparation controls

These are local diagnostic controls preceding the exact committed release build,
not substituted release-package qualification or universal human acceptance.
`LOCAL_BUILD.json` preserves the tested working-tree identity and executable hash.
The exact-source Linux/Windows receipts are produced separately by the release
preparation workflow and distributed with the release.

## Software-renderer timeout

The original CI failure is preserved verbatim in `original-ci-failure.log`:
Willow Town exceeded the generic 120-second allowance on the hosted software
renderer. The fixture deliberately executes 600 game seconds (roughly 12,000
rendered frames at its unchanged fixed cadence).

The unchanged original executable passed all 115 checks with a supported 640x480
viewport on Mesa llvmpipe in 70.868 seconds. Both application tests now explicitly
request that viewport and assert the actual window size. No gameplay duration,
input sequence, timestep, physical workload or acceptance threshold was reduced.
Fresh local native runs pass 116 original checks, 57 original cold checks,
151 life checks, 73 life cold checks and 26 absence checks. Raw logs and commands
are retained. These counts add one viewport assertion to the prior checks.

The release gate now includes the whole AI group, independent application cold
readers and reservation write/read processes. The source archive and SDK include
both AI learning projects. Packaging positive/negative controls cover that scope
and the readers' actual producer paths.

## Intermittent graphics anomaly

The original corrupt house/missing UI rectangles remain preserved under
`../willow-town-life-20261101/soak/`. Two fresh normal visible launches, wrapped
and unwrapped, produced correct native and external captures, ordinary saves and
clean runtime exits. `graphics-probes.json` records commands and image hashes;
the native images and raw logs accompany it here.

The unwrapped observer conservatively reported an incomplete protocol because
buffered stdout/stderr interleaved one diagnostic into a JSON observation. That
is not a reproduced renderer failure. No stdbuf cause, renderer fix or immunity
to recurrence is claimed. The original anomaly remains unresolved and currently
not reproducible; no speculative renderer change was made for this release.

Advanced Fluids remains locked in Alpha. This preparation does not unlock it,
publish a release or certify new hardware/platform combinations.
