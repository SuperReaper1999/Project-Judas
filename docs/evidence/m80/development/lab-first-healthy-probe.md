# First healthy lab probe: gravity-dominated plastic beam

The first actual application probe completed with healthy deformable instances.
Its A/B/oblique beams used density **10 kg/m³**, shear compliance **0.0005**,
volume compliance **0.000005**, and damping **1 s⁻¹**. B used yield strain
**0.04**, plastic rate **1 s⁻¹**, maximum plastic strain **0.4**. The additional
A/B load was **20 N**, with normal **9.81 m/s²** gravity still active.

Root's reported measurements from that probe:

- A loaded displacement: **0.7707 m**; unloaded: **0.5182 m**.
- B unloaded displacement after **4.3 s**: **2.587 m**, with plastic-rest change
  **0.2144 m**. Its own sustained weight dominated the initial plastic response.
- The panel's actual finite-mass rigid press produced a **0.0956 m** dent and
  **0.0273 m** plastic-rest change.

The following adjustment concerns ordinary authored demonstration content only:
A/B/oblique beam density becomes **1 kg/m³**; B yield strain becomes **0.15**;
the A/B additional end load becomes **40 N**. Their shear/volume compliance and
physical dimensions are retained. The cohesive structure and pressed panel
retain density **10 kg/m³**, and the panel retains yield strain **0.04**.

The purpose is to make the normal gravity-loaded baseline and the added-load
comparison readable. No engine mechanism, physics cadence, solver policy or
acceptance threshold is changed. The revised authored configuration requires a
fresh application baseline/load/unload measurement; this record does not claim
that it passed before that measurement occurs.

A second authored revision (density1, 40N, yield0.15) was healthy but did not
cross yield, so B did not visibly retain deformation. The independent material
probe (`lab-material-probe.cpp.txt`, `lab-material-selection.log`) measures calm
peak strain0.0244007 and loaded strain0.0760873 at yield0.04, no calm rest flow and
0.0182259m retained rest change. Final A/B retain density1 and 40N, with B yield0.04.
No numerical test threshold or solver quality changed.
