# Current deformables investigation evidence

No Deformables V2 implementation. `provenance.json` records the inspected engine
source hashes; `results.json` lists actual existing test executions. Short benchmark
scope and limitations are stated in the investigation report.

Reproduce the two new diagnostics from the repository root with an existing
Release **Unix Makefiles** engine build (as used for these observations):

```sh
cmake --build build --target judas_wheel_tests
python3 docs/evidence/deformables-v2-investigation/compile_probe.py build docs/evidence/deformables-v2-investigation/inertia.cpp /tmp/judas-deform-inertia
/tmp/judas-deform-inertia
python3 docs/evidence/deformables-v2-investigation/compile_probe.py build docs/evidence/deformables-v2-investigation/contacts.cpp /tmp/judas-deform-contacts
/tmp/judas-deform-contacts
```

The helper reuses the existing engine include/link configuration; it is not a new
runtime dependency. The inertia probe writes only its disposable `.cache` fixture.
The missing capsule/static-wake controls are observations of unsupported/broken
current behavior, not tests weakened into success.
