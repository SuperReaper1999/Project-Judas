# Renderer V2: packaged Linux SDK and relocated consumer qualification

This is a development SDK qualification, not a published release or Ubuntu
24.04 compatibility claim. The current host uses glibc 2.43. Root owns the
separate packaged-editor and Renderer Workshop qualification.

## Observed results

- Fresh SDK staging executed all 17 public-tool identity probes successfully;
  all matched `INITIAL_SDK_BUILD_INFO.json`. Package verification covered 697
  files before and after consumer operations. Host dependency inspection passed
  with no errors; it applies only to this host.
- Only packaged tools created and validated a new project and scene. The fixture
  uses a genuine managed MK2 box, a static floor, a project script and two input
  actions. A deliberately broken script failed with its asset location; restoring
  the script passed. No source project or installed package was modified.
- The packaged exporter produced a complete portable game. Its original export
  directory was moved away to the separately recorded `/tmp` location. Runtime
  bytes exactly matched the staged SDK. Processes launched from an unrelated
  empty working directory, with isolated user-data/config directories.
- The final strengthened fixture constructs `gold=0`, changes it to `37` during
  new-game initialization, then requests and completes a modern slot save. A
  separate cold process loads it and runs `restore`, which requires the saved
  value `37` to replace the different constructor default `0`. The earlier
  constructor-equals-save fixture is retained as a weaker, superseded state
  oracle. Native MK2 body identity 9 was part of the saved world. Packaged
  save inspection reports 13 participants; packaged compatibility checking passed.
  A missing-slot negative control rejected rather than claiming a successful load.
- Both hidden native processes rendered nonblank 1024x768 captures. Automated
  execution and captures are not human visual or gameplay acceptance.

## Negative evidence and limits

The initial restricted graphics invocation failed immediately with `x11 not
available`; `write.log` preserves that failure. The desktop-authorized native
reruns passed. The namespace prerequisite independently failed because this
host forbids unprivileged user namespaces. A subsequent successful hidden cold
load was traced with `strace -f -e trace=%file`: all 26,420 lines contain zero
references to the source/build/SDK checkout path. Scene, script and engine font
reads came from the relocated game; the compressed raw trace and summary are
retained. That trace exercises the earlier fixture with the exact same engine
executable; the stronger state-oracle pair was not redundantly traced. This verifies the exercised path avoids source-tree
reads, while remaining short of namespace/clean-machine certification.

The initial staged SDK contains the earlier Renderer Workshop script copy.
Root discovered and corrected that project's missing `caseKey` property schema
in its separate lab smoke. The final SDK must be refreshed from that corrected
project; these initial inventory receipts remain immutable evidence of the
package actually exercised by this consumer fixture. No Advanced Fluid unlock,
Windows/Android native qualification or release publication follows from this.

## Reproduction

From the checkout root, with matching Release tool binaries already built:

```sh
python3 tools/package_sdk.py stage --source "$PWD" --bin-dir "$PWD/build" \
  --build-info "$PWD/build/judas-build-info-Release.json" \
  --output "$PWD/.cache/renderer-v2/sdk" --probe-tools
```

Use a fresh generated destination if that SDK already exists. `qualify.py`
records the exact package-only authoring/export commands and generates the
consumer fixture; `qualify_gpu.py` records the initial hidden native write/cold-read
commands. `strengthen_cold.py` and `STRONG_COMPLETE.json` are the final differing-
constructor/save-state qualification, including a fresh export and missing-slot
negative control. They intentionally refer to the retained cache and `/tmp` relocation
record rather than replacing an existing published SDK. `RESULTS.json` preserves
all attempted operations, including the failed first graphics invocation.

`INITIAL_SDK_SHA256SUMS` belongs to the exercised initial SDK. The local
`SHA256SUMS.txt` below covers these retained evidence files and fixture sources.
