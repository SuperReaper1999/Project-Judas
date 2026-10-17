# Reproduce M94

From repository root, using Linux Release and an available native display:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
python3 scripts/m94_validation.py --build --output .cache/m94/reproduce
cmake --build build --target judas_script_vm_benchmark -j 3
python3 scripts/m94_delivery.py
```

The delivery driver reserves `.cache/m94/delivery`; use a fresh checkout/output
location if that owned directory already exists. Validation output directories
must be fresh to preserve prior receipts. `--start-at <target>` continues the
focused runner after a previously recorded invocation/setup failure. It is not
a blanket assertion that skipped tests passed.

The validation runner sanitizes inherited `JUDAS_*` test/profile variables, uses
dummy audio and isolated user-data storage. It runs ordinary SDL/OpenGL paths,
not a surrogate JS interpreter. A real display is needed for native application
checks; it is acceptable to run the headless source checker separately:

```sh
build/judas_script_check projects/lastlight/lastlight.judasproj
build/judas_script_check projects/developer_lab/developer_lab.judasproj
```

## Paired VM baseline

No build of the entire old engine is needed to isolate instruction-hook cost.
Use the exact accepted VM source and the same unchanged helper objects:

```sh
mkdir -p .cache/m94/baseline
git show 3306b5aa66a2e649bad06abc8c5cba452058d3fa:third_party/quickjs/quickjs.c > .cache/m94/baseline/quickjs.c
cc -O3 -DNDEBUG -D_GNU_SOURCE -DQUICKJS_NG_BUILD -std=gnu11 -Ithird_party/quickjs \
  -c .cache/m94/baseline/quickjs.c -o .cache/m94/baseline/quickjs.o
cc -O3 -DNDEBUG -std=gnu11 -Ithird_party/quickjs tests/ScriptVMBenchmark.c \
  .cache/m94/baseline/quickjs.o \
  build/CMakeFiles/judas_quickjs.dir/third_party/quickjs/dtoa.c.o \
  build/CMakeFiles/judas_quickjs.dir/third_party/quickjs/libregexp.c.o \
  build/CMakeFiles/judas_quickjs.dir/third_party/quickjs/libunicode.c.o \
  -lm -ldl -pthread -o .cache/m94/baseline/vm_benchmark
python3 scripts/m94_measure.py --output .cache/m94/reproduce-measure
```

Each process records ten batches of 100 identical calls, each with 15,000
arithmetic/branch iterations. Discard the first batch; alternate baseline/candidate
order across five rounds. Checksum trajectories must match exactly. p95 and worst
are distributions of **batch means**, not individual-step latency guarantees.
Both the preliminary and final observations are retained. The median changes
sign between observations: do not present this as a speedup.

## The actual edit/debug/fix case

Read [the project checklist](../../../projects/developer_lab/README.md). The original
project intentionally retains `calibration.gain = 0`. Consumer tests edit only a
private copy, inspect both gain values and use unchanged E input before/after.
The running production interpreter, not rewritten JS source, performs the calls.

For launch:

```sh
build/judas_editor projects/developer_lab/developer_lab.judasproj
.cache/m94/delivery/package/judas
```

The second command is the exported runtime; it has no editor/debugger panel.
The moved-package path/cwd and hashes are in `delivery/RESULTS.json` and
`delivery/PACKAGE_FILES.json`. Its intentional E-triggered teaching fault is not
silently repaired in the shipped source; export your edited project for a repaired
version. Package smoke validates normal preview, pause, reload and relocation.

The fingerprints cover current source, final binaries and intended checkpoint
files separately. They exclude generated executables/packages from Git staging.
