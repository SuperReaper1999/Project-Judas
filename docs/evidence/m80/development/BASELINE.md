# Preserved M80 baseline

The unchanged loaded fixtures were compiled against the preserved accepted M79
engine archive and matching HEAD headers before the M80 rebuild. The baseline
log records eight failed checks: three loaded orientation controls plus absent
soft structural-section preparation/strength/moment capability. Existing elastic
recovery, plastic retention, mass/rest-volume, deformed-surface and rigid contact
checks passed. The original fixture is retained verbatim here.

Baseline implementation: 3745787d0732a05e82aba9496e592bc2233564f0.
`material-frame-baseline.log` retains the original conditions/results. Isolated
HEAD `Deformable.h` and `FractureTypes.h` were required when linking this baseline
library. An intermediate attempt mixed new headers with the old library and
aborted from incompatible C++ layouts; that is test tooling error, not an engine
physics failure. No acceptance test, quality parameter or tolerance was weakened.

The baseline fixture tests actual loaded endpoints and authored strength controls,
not source locations or API presence. It uses 1/60 s, unchanged M62 numerical
material values, and bounded existing substep/iteration settings. Newly added
soft-section capability is intentionally absent at the baseline; those failures
are capability controls, distinguished from the three existing orientation bugs.
