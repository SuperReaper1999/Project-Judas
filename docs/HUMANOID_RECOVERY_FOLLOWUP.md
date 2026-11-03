# Deferred humanoid recovery

The protagonist recovery experiment is parked after visual review. The RPG
project keeps its existing recovery. Experimental checkpoint
`a6eeedbe0d7fae39d0dcbb57b5569a2e9853046b` corrects the scale-dependent ragdoll
mapping refusal independently of this experiment.

The physical prototypes established one-foot transfer and intermittent two-foot
support on the F8 and falling-crate fixtures. None could stand and balance.
The hybrid return reached grounded locomotion on both matched trials and refused
both obstructed standing destinations. A frame's difference in F8 timing could
still send it down the fallback path. Capturing in the destination capsule frame
removed most of the one-step jump, but the direct idle blend still splayed the
arms and let the feet slide or pass through the floor. Post-blend IK reduced the
worst F8 ankle dip from 9.4 cm to 3.7 cm below the floor. The motion remains
visually unsuitable.

On the development machine, the prototype sources, negative results, scenes
used, scores and reproduction commands are indexed by `HYBRID_IDLE.md` and
`GROUNDED_BLEND.md` under `.cache/humanoid-recovery-20261103/`. The test
launchers depend on disposable copies of the RPG project in `/tmp`. No
get-up controller or blend prototype was shipped.

When revisiting, test a short authored or procedural contact-aware rise first.
Keep a hand braced while moving the chest over a planted foot. Release the arm
as the hips rise, then blend to idle late. Evaluate rendered contact, hand and
foot travel, natural pose and capsule clearance before gameplay acceptance.
Retain both original falls, vary the F8 timing and include obstructed placements.
