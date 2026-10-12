# Exact-contact-existence parity

Status: **286,091 checks passed; 43,980 geometry samples**, including 6,874 accepted and 37,106 rejected contact sets. This is an isolated Linux verification of the proposed shared witness traversal. It does not imply final combined candidate or Windows acceptance.

## Verified contract

`CookedHasContacts(a, b, margin)` equals the original `CookedContacts(a, b, margin).count > 0` for the same represented inputs. A separately compiled oracle preserves the pre-refactor SAT/witness/reduction implementation. The refactored full manifold is compared against it field by field **bitwise**, including contact count, uncertainty, points, normals, separations, penetration, local anchors, local witnesses and separation states.

Coverage includes:

- All thirteen authored rig bone shapes and the unchanged cooked 8,192-triangle floor.
- Reversed argument order, one-sided/two-sided sheets and convex hulls.
- Sphere/capsule fallback paths; rotated boxes, child offsets/orientations and non-unit represented quaternions.
- Negative, zero, minimal positive, 0.001 and 0.2 m margins.
- Float `nextafter` positions around touching, positive separation and penetration.
- Coplanar internal edges, convex/concave creases and reversed winding.
- Deterministic seed `0x4d783843`, arbitrary orientation and large local coordinate offsets.
- An independent clipped-face witness-gap calculation using the existing explicit two-triangle sheet fixture.

Only Boolean truth requests use the short circuit. Proximity advancement and physical impulse rows continue to use complete manifolds. The existing manifold consumer retains original accepted-witness order and physical contact data.

## Failures preserved and explained

1. **Probe compile attempt:** `SceneSerialization.h` forward-declares `Scene`; the initial probe omitted `Scene.h`, producing incomplete-type / missing `RagdollShape` errors. Adding the ordinary header corrected the probe, with no runtime change. Original build/logs remain in `parity-compile-attempt1/`.
2. **Independent bonus assertion:** the first large-grid probe incorrectly required every SAT edge-fallback gap to equal witness-plane distance. That identity belongs to clipped **face** witnesses. Edge fallback intentionally retains its separating-axis gap in the original and corrected implementations. The original run failed after 43,380 samples / 279,147 checks; its complete manifold and Boolean parity checks had not failed.
3. **Diagnostic follow-up:** `../parity-gap-diagnostic.log` shows the same accepted contacts reporting SAT gaps 0.199589 and 0.0197274 m while witness distances are 0.179122 and 0.0996783 m. The independent original oracle returns the same manifold bitwise. This is an invalid probe assumption, not a changed production acceptance criterion.
4. **Corrected bonus proof:** all 43,980 seeded grid samples remain unchanged. The independent face-plane identity is tested on the existing explicit two-triangle face fixture. The corrected run passes 286,091 checks, including all negative sets. No contact/CCD threshold, solver limit, body property or production physics was retuned to achieve parity.

`parity-assertion-attempt1.cpp`, original failed run/build receipts and the diagnostic are retained. `contact_existence_parity.cpp` contains the narrowly corrected probe. `../contact-existence-parity-corrected.log` / `.json` and build receipt pin the exact passing source, archive and executable hashes.

## Suggested small integration checks

Add a compact predicate/full-manifold comparison to existing `CollisionGeometryTests.cpp`, without changing its historical oracles:

1. Existing inclined-box/two-triangle fixture: zero/0.2 m margins, both argument orders; preserve each point's existing independent witness-depth checks.
2. Separated box above the same floor: false at zero and a small positive margin; both orders.
3. One-sided sheet backside versus equivalent two-sided sheet: explicit negative/positive truth.
4. Existing sphere, capsule and hull fixtures: Boolean result equals full manifold count, both orders.
5. Existing rotated-child/oblique configuration: equality of Boolean and full manifold truth.

These bounded tests detect obvious drift in the shared seam. The larger independent original-oracle probe remains the stronger parity evidence. Re-run exact unchanged four-rig observation and affected contact/CCD suites after production integration; do not count isolated results as combined acceptance.

All relevant hashes are recorded in `MEASUREMENT_FINGERPRINTS.json`; raw binary/archive hashes also appear in their build/run receipts.
