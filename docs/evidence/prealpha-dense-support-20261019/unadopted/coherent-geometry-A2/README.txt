COHERENT GEOMETRY A2: REJECTED ISOLATED EXPERIMENT
================================================
No production changes were adopted. The safe 8f Experimental checkpoint and its
committed report/evidence remain untouched. Main remains accepted e30.

Same native 270-step, unprofiled workload:
                             safe fastpaths        coherent A2
mean fixed step ms              63.03256             96.68418
median                          23.00445             26.18670
p95                            213.60452            416.01414
worst                          942.04712           1299.07793
impact events                     3520                  3366
impact queries                6608132               5899861
impact search iterations       511168               3771303

A modest reduction in events/queries did not translate into lower total cost.
The larger search-iteration count and actual slower measured time rule out
adoption. This is a single paired observation, not a significance claim.
Both runs have 270 steps and no nonfinite recorded rows; trajectories differ.
Endpoint geometry is recorded, not substituted for full physical qualification.
A2 native endpoint deepest penetration 6.711 mm versus original 7.217 mm does not
make the overall experiment successful or prove every intermediate state safe.

The exact physical/work comparison exits 1 as it should for changed trajectories.
The raw failure is not relabelled an exact-physics pass. Root stopped broader
qualification because the performance purpose failed: no full 34-case run,
M70/M77 cold save or temporal-quality world run is claimed.

Independent public motion-history concern
----------------------------------------
One unchanged diagnostic source was compiled separately against each header and
archive pair. Baseline exact reconstruction passed all 384 observations (96 free,
96 kinematic, 192 impact). A2 free/kinematic histories also reconstruct exactly,
but impact history reconstructs only 113/192 exactly: 49 position mismatches and
79 rotation mismatches across 16 genuinely split bodies/events.
Largest position error 4.8060576388e-7 m; unit-radius rotation-point displacement
1.5579018514e-7 m. Stored endpoint repair is 0 in both variants, so final position
repair is not the cause. The candidate's --require-exact returns 1; baseline 0.

The prototype retains double event anchors internally but exports float-valued
public segment anchors. Replaying those rounded anchors cannot guarantee exact
reconstruction of the retained post-impact path. These are small discrepancies,
but the original public history's authoritative anchored-integrator description
cannot silently be treated as exact for this representation. No tolerance or
public API was changed to disguise the result. Original source/build/raw logs
are under public-history; this is not a claim every persistence path failed.

Support-proof failure and correction
------------------------------------
The ORIGINAL controls receipt denotes the broken precision-A1 revision, not the
original production 8f engine. Its extent-only projection accidentally counted
relative centre twice and failed 3 of 9 native support checks. Corrected A2 passes
9/9 support checks and 48,057 projection checks; the deliberately restored double-
centre error fails 3 detecting checks. Those positive/negative observations remain
separate from the rejected whole-world performance outcome.

The same-source original 8f control independently passes all 9 checks with the
original headers and frozen safe archive. SUPPORT_CONTROLS_SAFE8F.json preserves
that positive baseline, distinct from the introduced A1 failure and A2 repair.

Preservation
------------
The snapshot source archive/FROZEN manifest and ABI receipt retain exact variant
bytes and hashes. Immutable native binary/archive remain in the separate
coherent-geometry-candidate-A2 snapshot rather than being duplicated here.
All raw per-step/frame rows, endpoint geometry, exits, limits and native logs are
copied under baseline/candidate. The complete current disposition is in
DISPOSITION.json. No protected history or independent fluid research changed.
