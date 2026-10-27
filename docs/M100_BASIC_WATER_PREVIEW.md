# Basic Water editor depth-order correction

Human review of the Alpha editor found that Stillwater's water disappeared over
its pool bottom until moved away from terrain. The scene file was not defective.
`DrawAuthoredScene` submitted each water surface immediately in authored object
order, with blending enabled and depth writes disabled. Opaque geometry submitted
later could therefore overwrite the water even when physically behind it. Play
already renders water after opaque geometry.

The authored preview now completes opaque submission before its water pass.
`DrawAuthoredWaterSurfaces` provides that pass separately for composed editor
views, so neighbouring-region terrain also finishes before any water is drawn.
Normal depth testing remains enabled: nearer land/objects still hide water.
Materials, render layers, visibility, the project water switch and arbitrary
surface orientation are preserved. Scene transforms and physical water/swimming
volumes are unchanged. Advanced Fluids remains locked off in Alpha.

## Focused evidence

`tests/BasicWaterPreviewTests.cpp` renders actual pixels through the ordinary
preview API. Before the correction: 12 checks, 4 failures. Water-first versus
water-last scenes differed by 87.573891 mean RGB units, and water over submerged
terrain produced zero visible difference from the water-disabled control.
After correction: 18 checks, zero failures; object-order difference is zero.
Positive and negative cases include visible water over submerged ground,
above-water occlusion, rotated/translated scenes, composed regions, render-state
restoration, hidden/disabled water and camera layer masks. New controls extend
coverage; the original failed expectations are unchanged.

The existing runtime-rendering suite passes 54 checks; the Basic Water gameplay
application passes 48, including swimming, pause and absence of advanced work.
This is native Linux/NVIDIA validation. No new Windows or Android result is
claimed for this editor-only follow-up. The test is registered in the water and
onboarding release groups. Raw failures, corrected output and screenshots are
retained in [the evidence](evidence/m100/basic-water-preview/).

No physics or fluid solver changed. No water was lifted, terrain hidden or depth
testing disabled to conceal the problem. Main remains operator-controlled.
The sealed `31205c6a` packages predate this correction; refresh final release
packages from the accepted follow-up checkpoint before publication. The updated
local Alpha editor can be human-reviewed before repeating distribution builds.
