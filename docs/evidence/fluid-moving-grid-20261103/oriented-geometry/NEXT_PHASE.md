# Oriented phase geometry feasibility

The static box builder already clips an oriented polyhedron to each cell
and can clip that result to one fixed water plane. The swept builder
integrates full shared apertures, blocked-face first moments and oriented
wall moments along a prescribed motion. Its pressure consumer explicitly
requires one uniform phase. The existing two-phase operator reconstructs
local planes and donor apertures for translating axis-aligned boxes.

## Bounded extension

1. Reuse each exact box–cell intersection. For candidate height \(H\),
   integrate free volume below \(H\) by adding one plane clip. The result
   is a monotone piecewise-cubic storage function. Its breakpoints come
   from the clipped polyhedron's vertex heights, which need not be
   original box vertices. Invert that storage once per pressure interval
   from the actual water inventory. Preserve exact empty/full traces and
   define the same deterministic lower solution on a zero-storage plateau
   as the axis-aligned reconstruction.
2. Keep the total opening of each grid face shared. At each sweep sample,
   clip the blocked face polygon against either adjacent cell's frozen
   phase plane. Subtract the result from that side's unobstructed wet
   rectangle. Face-normal cuts use the correct one-sided traces. A full
   lower donor is wet at its upper face even if the upper receiver is dry.
   Use a single signed material flux selected from the donor side.
3. Keep pressure perturbation work tied to the same integrated solid
   moments and total endpoint capacity as the projection. For a fixed
   global hydrostatic reference, exact water and air background forces and
   torque can be evaluated from displaced phase volumes and their first
   moments. This uses the divergence theorem and avoids computing every
   wall-patch covariance at every time sample. The interface pressure is
   continuous and the phase cap is never a solid wall.
4. Qualify a fixed-reference crossing before using independently evolving
   planes. Move the material inventory through the existing phase flux
   contract. Reconstruct again from that inventory after each accepted
   interval.

## Conservation and scope boundaries

Let \(C_c\) be a cell's free capacity, and let \(S_f\) be the signed solid
volume swept across a common grid face. Total geometric conservation is
\(\Delta C_c = \sum_f S_{f,\mathrm{out}}\). For volume restricted below a
fixed plane, the outward sum also includes the solid sweep across the
plane inside the cell. Omitting that term would reject real crossings or
hide displaced water. An evolving phase plane adds a separate moving
boundary term. The proposed first interval freezes it.

A single perturbation pressure per mixed cell can transmit dynamic
loads through its represented air. Exact dry classification prevents
an incorrect hydrostatic water load, but does not establish that the
dynamic pressure split is accurate. Keep ambient air pressure and
water traction separately measurable before changing the pressure
unknowns. Entirely subcell motion can leave the total apertures and
capacities unchanged; a scalar cell pressure cannot resolve its added
mass or internal redistribution. Small disconnected pockets in one cell
also cannot carry independent water levels.

Angular sweeps near coplanarity can have narrow wet intervals that
quadrature misses. Current total face and wall conservation cannot
certify wet aperture where the solid crosses no grid face. Use a separate
wet-volume check that includes the phase plane. Seed angular vertex
crossing events or report uncertainty when an aperture near grazing
does not converge. Reusing cached cell and face clips matters because
the swept controls already make hundreds of exact spatial samples.

## First controls

- Oblique box 4 mm above water in a partially wet cell. Both static and
  prescribed motion stay dry. A companion immersed control must report
  the independently expected water force. Air-only loading is measured
  separately.
- A yawed prism translating through the level, with analytic submerged
  volume and buoyancy over time. Check wet and total capacity, and count
  the phase-plane swept flux as well as common grid-face fluxes.
- A rotating body that becomes wet between dry endpoints. Use a refined
  time reference for nonzero wet duration. A frozen or endpoint-only
  classifier must fail this check.
- Two adjacent independently reconstructed levels, with a solid clipping
  their shared face. Check each wet trace, a single paired donor
  debit/credit, full-donor flow through the upper face, and no passage
  through a genuinely closed aperture.
- Constant pressure gauge and nonuniform pressure work in the same
  geometry. Reject missing angular work and a pressure gauge that changes
  closed-body net load. Track unresolved subcell motion and split wet
  pockets separately from accepted cuts.
