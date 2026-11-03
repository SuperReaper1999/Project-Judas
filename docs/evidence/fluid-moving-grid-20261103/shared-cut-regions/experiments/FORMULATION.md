# Wet-only vented cut control volumes

This note proposes a bounded pressure and transport formulation for the retained
rectilinear chart. The scratch two-storage calculation exercises its discrete
accounting. It does not qualify the authored basin, MK2, or the runtime.

For every connected wet part of a cut region, store water volume W and construct
the wet region, wet face intersection, vented water surface, and solid-wall
patches from the same solid and phase geometry. A pressure unknown belongs to a
connected wet part. A moving solid or water plane can disconnect water even when
the full nonsolid region is connected. Reject those cases until the chart can
split them. Dry regions carry no inertia, velocity, pressure unknown, or material
ledger. Trace the dry component through positive-area dry openings to an explicit
ambient boundary. A trapped or unclassified headspace is outside this model.

Use a chart aligned with constant gravity in the first gate. Subtract the exact
hydrostatic field p_H(x) relative to a common reference head. Solve perturbation
pressure pi and reconstruct total pressure p_H(x) + pi. At an atmospheric wet
surface, pi equals minus p_H at that physical surface. With the same canonical
reference height, a stationary connected lake yields exactly zero predictor and
perturbation, independent of dry cut-volume sizes. A connected actual side leak
below the reference head does not describe such an equilibrium.

Let B orient each wet face out of its left cell and into its right cell, q be
one signed water volume transfer per face, and s be each cell's signed volume
swept by its actual vented surface. Let C_i^end(h) be water capacity below level
h in the actual ending solid geometry. For a same-owner trial with starting level
h_i, set W_fixed = C_i^end(h_i) and r = W^old - W_fixed. Then

    B q + s = r
    q = q* + C B^T pi
    s_i = s_i* + C_i,s (pi_i - pi_atm,i(s_i))
    W^new = W_fixed + s = W^old - B q

The face coefficient is dt^2 A_w / (rho ell_w) for the stated normal model.
The surface coefficient uses its physical area and actual wet dual length.
Compute pi_atm from the discrete gradient of the storage potential. This is the
change of the independently integrated potential divided by s, with the analytic
derivative at s=0. Its derivative is nonnegative for positive surface storage.
Eliminating s adds a positive diagonal at each vented owner. If a surface moves
beyond that owner's geometric chart, stop at a topology event and rebuild with
the connected new wet geometry. Positive side discharges into adjacent dry
regions need a same-interval deposited water owner. They are not atmosphere
sinks. Until implemented, reject their dynamic transitions explicitly.

From the pressure law, the work identity is

    sum((pi_L-pi_R) q) + sum((pi_i-pi_atm,i) s_i)
        = sum(pi_i r_i) - sum(pi_atm,i s_i).

The left side equals the change in kinetic energy plus the backward-Euler
correction energy for the metric defined by the face/surface coefficients.
The last term on the right is the free-surface potential energy change. For
known wall motion, integrate perturbation traction on wet wall patches so wall
and fluid work have opposite signs. Integrate the hydrostatic baseline over the
same wet patches with first and second moments for force and torque. If a patch
has zero wet area its water traction is exactly zero, including the 4 mm
negative control. Avoid using a coarse wet-cell flag or a second buoyancy force.

Retain the existing thin inviscid aperture control. For a physical viscous
parallel slit, a separately qualified optional term alpha = 12 mu ell / b^2
adds dt alpha to the coefficient's denominator and alpha A dt u^2 to the
dissipation ledger. This offers physical attenuation for actual sub-resolution
wet seams without deleting their aperture. It needs its own geometry-derived
gap and viscosity definition. It cannot substitute for wet topology.

A prescribed-trajectory positive control can precede native body feedback. If
the wet patch Jacobian is G, with r = G d for small generalized displacement,
the dual pressure load is -G^T pi. Eliminating backward-Euler rigid motion
adds dt^2 G M^-1 G^T to the Schur matrix. This addresses the independent
added-mass iteration failure and requires torque-aware geometric ownership.

First gates

1. Aligned and rigidly rotated exact equilibrium on a sealed liquid basin
   vented above the water line. Verify canonical head, zero water velocity,
   exact dry-gap load, known force/torque, and a trapped-gas negative.
2. Keep the exact authored seed and geometry as a separate case. Report the
   water-side seam leakage and its local transit restriction. Preserve the
   original mixed pressure and transport failures as controls.
3. Two connected wet volumes with genuinely cut surface storage and a wet
   aperture sweep. Verify paired signed water flux, local storage, volume and
   work. Advance across a coarse cell with a real new wet owner.
4. Prescribed wall/body trajectories, then simultaneous rigid pressure
   coupling. Require force, torque, momentum, fluid and body work, and
   transactional rejection before native feedback testing.

Evidence

Run `python3 two_storage.py` from this directory. The small graph uses
a scalar Schur solve on two storage curves. A real 3D chart assembly, side
emission, gas-vent construction and MK2 feedback remain untested.
