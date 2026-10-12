# Declared before correction results

PHY-002: unchanged 900-step, 60 Hz one/four rigs and triangle grid; target four-rig median below 8 ms, p95 below 16.67 ms, maximum below 50 ms on the isolated development host; one rig proportionately cheaper. Settling must follow normal sleep predicates, not forced sleep. Cold-impact spikes reported separately. Partial improvement does not close the finding. Existing supported fast/edge/overlap/filter cases must still pass.

PHY-001: torque-free world momentum vector relative error below 1e-4 over two seconds; energy error must converge with timestep refinement, expected second-order orientation transport if implicit midpoint is selected. Quaternion norm within 1e-5. Sphere, rotated state, torque and impulse independently checked. Solver contact/constraint residuals separate from free-body conservation.
