# Engine-archive qualification failures and corrections

These are genuine development failures from checkpoint
`9b03d9a6bb3f4736d11cd08e5fbb686faed90904`, not final candidate passes.
The Ubuntu 24.04 SDK rebuilt all engine/tool/test objects after discovery that
reusing objects from an older source archive with deterministic zero timestamps
can leave stale code. Only unchanged pinned dependency builds were retained.
The first coordinated gate then passed 36/45 cases. Its nine failures are retained.

Four tests wrote relative scratch files in the intentionally read-only source
mount. Three test families required consumer projects omitted by the engine-only
source profile. The correction runs tests in writable evidence output with an
explicit engine asset root and includes frozen project fixtures. Original project
content, simulation settings, inputs and numerical thresholds are unchanged.
The gate has an actual child-process positive/negative scratch-path control.

The lesson's cold-save endpoint assertion sampled after resumed gameplay rather
than at publication. Observer-only instrumentation linked against the unchanged
checkpoint engine proves zero positional error and exact velocity at restoration;
the later UI sample is 0.239877 m farther along. The test now checks native pose
and velocity before the next UI/fixed-step update. Its original 0.03 m threshold
is retained. The fresh-process cycle and post-restore physical drive still pass.
No save or physics implementation was altered to hide this observation error.

Follow-up hardening rejects reuse of a different clean source archive in an
already identified build directory; validates maturity/SemVer agreement; bounds
untrusted JSON nesting; and verifies embedded source-archive inventory bytes in
addition to an outer receipt. Synthetic metadata/package tests are not native
platform qualification. Final combined-candidate qualification remains separate.
