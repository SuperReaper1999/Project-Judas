# Vehicles V2 evidence

Engine code: 1afc3ff23833c72ede30e86d2d8ae1a9b70d7adb. These are automated results, not operator acceptance. Later documentation-only commits do not change the tested implementation.

The pinned cd4 baseline and V2 were built independently with GCC 15 Release. The same benchmark source and fixture hashes are recorded in the provenance files. Three fresh process pairs alternated ordering, pinned to CPU 2, after compiler jobs ended. Each non-impact case has 1080 retained samples per process; impact retains all 1200. Tables use the median of run percentiles and largest observed worst. No speedup is claimed: improved contact and coupling require more work, and rough-course trajectories deliberately differ.

Runtime logs have profiling disabled. Profiled phase samples have identical physical outcomes, verified against the unprofiled runs; diagnostic recording is not a simulation input. Phase scopes are nested and cannot be added to obtain total cost. Source for the matched powered fixture is retained in the local qualification folder alongside compiler commands, exact binary hashes and rejected controls. The tracked manual benchmark is tools/benchmarks/VehicleBenchmark.cpp.

Native qualification preserves the five original environment failures and unchanged-binary retests. CI independently ran the whole selected gate suite on Ubuntu 24.04 with Xvfb and socket access: Experimental 178 passes; Alpha 174 passes and four correctly unavailable locked-fluid cases. The two-channel job and baseline SDK checks are linked in qualification.json.

Packaged-SDK testing hid the entire development checkout and mounted the SDK read-only. Its fresh-project/editor workflow and 39 tool commands succeeded, including expected negative controls. A relocated Vehicle Workshop then saved and loaded in separate processes with its SDK and source project also hidden. The namespace used software graphics; separate desktop runs exercised the GTX 970.

The first namespace attempt lacked private device nodes and blocked a child-tool probe. Correcting the test environment resolved that without engine changes. Earlier mixed-source build identities, unsupported SDL offscreen resizing, restricted socket runs and the origin-chart experiments remain in the local raw records rather than being relabelled as passes.

Local full records: .cache/vehicles-v2/. SDK build and selected revision are recorded in its BUILD_INFO.json and SDK_MANIFEST.json. No native Windows, Android-device or browser execution is claimed for this change.

The exact powered benchmark sources and compile helper are retained here. Their absolute fixture include path records the original run; on another machine adjust that path to the pinned current fixture. Compile against each build’s own generated WheelTests flags/link libraries, then alternate the two executables with no compiler jobs running. The helper records source, fixture and binary hashes. The profiled and unprofiled variants differ only in diagnostic collection and its output label.
