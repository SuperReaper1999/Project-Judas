# Native actual endpoint observer

Compile only `FinalCurrentStandingObserver.cpp` using the unchanged original BalanceLab test compiler flags plus the normal third_party include. Link its explicit object in place of the original BalanceLabTests.obj against the frozen final native71a engine archive and the usual original dependencies. Do not replace any engine object.

Use a NEW owned output directory; set `JUDAS_DIAGNOSTIC_CONTINUE_WRITE=1` in that process environment. Clear every other `JUDAS_DIAGNOSTIC_*`, `JUDAS_BOUNDARY_*`, and `M78_DIAGNOSTIC_*` variable. Run `cold-write`, then a separate `cold-read` on the SAME directory. Original write runs24checks/read13checks unchanged. The write additionally continues the exact captured live world for720steps; no saved data is modified.

Copy `physical-final.json` immediately after each run to separate immutable `source-physical-final.json` / `restored-physical-final.json` files before the next run replaces it. Preserve save/source/executable/archive hashes. `currentPhysicalEndpoint` reports current History.progress/reason, physical root pose/velocities, mass-weighted body state, directly sampled gravity, actual completed foot samples and a derived verdict requiring Succeeded plus unchanged physical standing limits. It does not consult cached Standing/Observation for that verdict.
