# Final-review physical frame corrections

Two adversarial checks exposed missing invariance, rather than a request for
new gameplay or solver retuning. Original fixtures and failed logs are retained.

## Reference-area rounding and translation

The cook accepts relative authored-area rounding up to 1e-6. Accumulating
centroid weights using that nominal area without normalization gave weights
whose sum differed from one. A common world translation then moved the measured
centroid relative to the actual section, producing a false bending moment.

With unchanged 120-step, 60 Hz loaded specimens, 5e-7 authored-area rounding
and translation (10000, -20000, 30000) m, physical positions agreed to
2.72241406e-10 m but peak section stress differed by 14.396492 Pa. The test's
unchanged comparison is 1e-4 Pa. Normalize the derived weights and sum the
centroid relative to its first material midpoint. Nominal strength area and
section moments are unchanged. No new physical state or asset layout is needed.

## Historical material frame and cold continuation

Changing a logical owner transform does not reset its already simulated nodes.
A cold instance starts from the current saved owner transform, while the live
instance retains its historical constitutive/plastic reference. That original
frame was not persisted. The direct one-step loaded continuation differed by
0.000124870483 m (unchanged comparison 1e-12 m).

The generic repair stores and validates the original orthonormal material basis:
new whole-world deformable participant version 3 and versioned retained instance
state. Existing native layouts remain readable with an explicit legacy path;
missing historical frames cannot be inferred after owner rotation, and exact
retroactive continuation is not claimed. No controller, material, timestep or
iteration setting changes. The pre-fix test executable and engine archive were
retained in /tmp; their hashes are below for provenance (binaries are not committed).

- test: 83b30419b1941ec02aedfb0419f9395eabf86ce65574250376085738ff27c0bc
- archive: 524cb5585e149d5d212de7fd272e79714e998c8197ef60325a39e86a13bad66b
