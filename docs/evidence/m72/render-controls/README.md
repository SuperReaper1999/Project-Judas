# M72 native rendering observations

Final run: **54 checks, 0 failures** in
`docs/evidence/m72/development/render-controls-02.log`.
The first run's five test expectation/setup failures remain recorded in
`render-controls-01.log`: imported colour expectations assumed a different
display transform, and repeated reloads waited before requesting the released
resource. The test corrections use saturated channel and unchanged-neighbour
observations, plus the ordinary request/wait/render sequence.

## Environment and coverage

The ordinary hidden EngineHost used SDL's offscreen driver, Mesa llvmpipe
(LLVM 21.1.8, 256 bits), software OpenGL, dummy audio and a 256 by 192 main view.
It began with cold skin/material shader paths. The auxiliary camera fixture used
128 by 96 pixels and the existing shared main-focused shadows and update path.

Actual colour and depth readback cover instance Blend/Mask routing, no blend
depth writes, cutout shadows, borrowed texture alpha, first emissive-map colour
view creation, sun enabled/intensity/colour/direction/reset, entity/component/part
visibility, retained collision bodies, independent shared-material instances,
pending/failed/empty/cleared texture replacements, auxiliary camera coherence,
resource retirement/reload and complete observed texture teardown. The floor
shadow probes changed from west/east **8/236** to **236/8** after reversing the sun.

The original multipart fixture uses a normal cooked model archive with stable
part keys. Its targeted image shows only the first part of the first instance
red; the other part and second instance retain their shared green source.
The restored image shows both instances back at the source appearance.

## Bounded workload measurement

The same process submitted 128 primitive instances sharing one material handle.
The default observation used 20 frames with all 128 visible. The changing
observation used 60 frames, changed factors on all instances each frame, kept
96 visible, and disabled the sun on 6 frames. Lower changing-workload times
reflect different visibility/shadow work; these numbers do not establish a
general speedup or a frame-rate guarantee. Timings measure CPU wall time around
control updates and rendering submission, including any driver waits, with the
existing M56 recorder enabled. No screenshot readback is inside these loops.

| Observation | Mean ms | Median ms | p95 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: |
| Shared defaults, 20 frames | 3.5640 | 3.5132 | 3.9301 | 3.9955 |
| Frequent factor/sun/visibility updates, 60 frames | 2.9561 | 2.9693 | 3.5599 | 4.0003 |
| Replacement prepare/two submissions/retire, 8 cycles | 7.2925 | 7.2704 | 8.0683 | 8.0683 |

Cold world/shared-source residency preparation took **1.5242 ms** and created
one texture upload, zero mesh uploads and one referenced shared material handle.
Disk cache state was uncontrolled. Steady frames created **zero new texture or
mesh uploads**; appearance cache bytes remained **540672**, and shared material
handle **4** remained unchanged. Eight replacement/retirement cycles created
**16 textures and destroyed all 16**, including cached colour views.

`shared-instance-profile.json` contains 80 M56 frames with zero dropped events,
exhausted registries, incomplete scopes, truncated records or GPU drops. Delayed
GPU records are included as recorded; the final two remain pending. The CPU
figures above do not use those GPU records as a timing guarantee.

Generated fixture sources remain under `.cache/m72/render-controls/fixture` and
are intentionally absent from this evidence directory. The PNGs here are direct
renderer captures; they have not been edited.
