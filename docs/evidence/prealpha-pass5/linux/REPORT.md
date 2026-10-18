# Pass 5 final-engine Linux qualification

## Executed scope

Runtime implementation: `be621491a6a1a578ac87e8c82e3b297ed8b18614`.
Qualification source checkpoint: `139d0de20e2e443bd186a6ed8f786eaa223cbba6`;
later CI test/workflow and report changes do not change compiled runtime sources.
Actual build ID is `0.1.0-prealpha-Experimental-139d0de-clean-b271ec3d7937-u84f47c24fcd0`.
Its 16 disclosed pre-existing untracked source observations make this development
qualification, not a clean release preset. Source/runtime hashes are recorded,
not retroactively relabelled as the later evidence commit.

- Fifteen real fixture processes pass **383 assertions**: origin physics13,
  assemblies46, streaming23, composed terrain/lifecycle34, persistence27,
  particles11, M70 actual98 and cold writer/reader10/14, M77 step24/13,
  standing23/13 and recovery21/13. Fresh readers validate physical endpoints,
  actual foot supports, height, velocity and authority, not just controller enums.
- Current-identity focused gate: candidate cache17 and continuation112 pass;
  contact lifecycle additionally passes1076. Earlier 11-fixture32614-check
  qualification uses the **byte-identical final engine archive** and is preserved
  separately in [dense-linux](../dense-linux/REPORT.md), not claimed reexecuted here.
- Real Spring Range scripts pass36 checks; the normal asynchronous application
  passes14, including target hit/score, camera switch, pause/fire isolation,
  resume and actual reload. Lastlight's original project starts, publishes its
  scene, toggles view and produces a screenshot without runtime script faults;
  all tracked game inputs remain exact. This is startup/presentation coverage,
  not new ship-flight, wall-pursuit or campaign human acceptance.
- Actual host/direct/impaired client processes pass14 network assertions. The
  proxy adds75ms directional latency,15ms jitter, omits every fifth application
  snapshot and forces one TCP break. Physical host impulse/motor movement,
  reconnect/stable identity and replica endpoints are exercised. This is **not**
  IP packet-loss emulation or public-Internet/security certification.
- All17 staged SDK tools retain actual identities/checksums/ELF dependencies.
  Script syntax positive/negative, model inspect, mesh/collision cooking,
  save identity and volumetric create/brush/validate/cook execute normally.
  Editor authoring/undo/duplicate/PlayStop/save restores exact authored bytes.
  A real four-scene export runs after relocation to `/tmp/Judas-PreAlphaPass5-Physics`
  from an unrelated `/tmp` working directory.

The whole GCC engine archive is still
`0e0dc7973b7d2b24510743d5297e9ae002cdb7b4dbdc943d55a4bdc4aab78514`.
Only the impact-event object differs from the clean accepted4c numerical archive;
no new controller, body integration, constraint, geometry, fluid or script API
change is hidden in this qualification. Exact object-member proof is in dense-linux.

## Performance gate, not a general speedup claim

The unchanged M78 four-articulation observer runs four900-step cases at60Hz;
mesh-4 has52 bodies and8192 floor triangles. Existing16.67ms p95 gate passes:
**4.183063ms p95 /13.130335ms worst**, observer sanity true. Two pre-existing
bounded CCD-search-limit observations remain. The earlier Pass4 result was
4.814678/15.280967ms; these are separate gate observations, not a matched speedup
experiment. This does not fix room1024's severe1.7–2.0s tail.

`FOUR_RAGDOLL_RESULTS.json` and original source hash identify the actual fixture.
Full profiles are losslessly compressed; `COMPRESSED_INPUTS.json` identifies the
original bytes and hashes. No timestep, mesh, body count, controller, iterations
or acceptance threshold changed. Windows was suspended and other owned heavy
jobs were held during this Linux timing window.

## Preserved failure and verification follow-up

The initial network attempt fails in the sandbox. An independent socket call
records `EPERM` in `SANDBOX_SOCKET.json`; the host cannot publish readiness. Its
original logs/failed aggregate remain in `network-process/`. The unchanged
native-socket replay is `network-native/`, with14 checks passing. This was not
masked as a passing command or mislabelled an engine regression.

Independent review found three evidence-runner weaknesses. Actual Lastlight logs
were rechecked against the real `script asset=` fault prefix, scene-ready marker
and actual screenshot, including ordinary Error/RangeError/string-throw detecting
controls. Normal Python execution was used; future runners explicitly reject
`-O`/`PYTHONOPTIMIZE`, and three refusal commands are recorded. Run calls now refuse
to overwrite existing logs. The as-run script bytes remain beside the improved
reproduction copies; this is a post-run assertion follow-up, not an engine rerun.

The original SDK loader receipt invokes the source inspector. A separate actual
**packaged inspector** receipt and subsequent strict SDK verification both pass,
covering the installed entrypoint without bytecode/environment masking. Earlier
source-only inspection is not silently relabelled packaged coverage.

## Portability and limits

GCC15.2 Release on the recorded i7-4790 Linux host. SDK ABI maxima remain glibc2.43,
GLIBCXX3.4.32/CXXABI1.3.15; older distributions are unqualified for this host-built artifact. Separately,
the clean050016 hosted Ubuntu24.04 SDK passes its declared max-glibc2.39 check;
its own receipts are in ../ci-followup/, not inherited from this host. Graphics used
llvmpipe/offscreen software rendering. Audio is dummy/unavailable; actual pointer
capture, physical GPU/audio/controllers and human usability were not validated.
The original curved372/4 and liquid knots failures remain in their dedicated
reports; FLU001 is unchanged and not rerun. No full production-suite claim.

`RESULTS.json` is the complete command ledger, including failed sandbox setup and
intentional negative exits. `SOURCE_FINGERPRINTS.json`, `BINARY_FINGERPRINTS.json`,
`BUILD_INFO.json`, physical endpoints, package receipts and screenshots retain
actual source/binary evidence. `reproduction/` contains executed and improved
bounded runners; use fresh output directories and the documented configure first.
