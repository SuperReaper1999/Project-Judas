# Planetary lab horizontal look — Experimental follow-up

The operator accepted candidate `2ea9587b2fbc734cbe2ec906ba344cd6d9ae2a67`
while reporting inverted left/right look. main was fast-forwarded through the
documentation-only acceptance checkpoint, preserving that exact implementation.
This follow-up is separate and is not claimed human-tested or promoted to main.

Cause: project input maps positive mouse:dx to positive look_x. The lab constructs
forward as base*cos(yaw) + right*sin(yaw), so positive yaw turns right. Its update
incorrectly subtracted look_x. The correction changes only that subtraction to
addition in Assets/scripts/explore.js. M35 input, engine camera/origin/physics,
vertical look and all bindings are unchanged.

The actual project class and math module execute in the focused Node check with
supplied logical input. Before correction: 18 assertions, six direction failures.
After: 18/18 pass across uniform-up, wall-up and an oblique gravity direction,
including left/right, tangent direction, modal suppression, vertical sign and
instance independence. This is project-JS evidence, not a native mouse/hardware
certification. No C++ rebuild or full-suite rerun was needed.

Reproduce: `node scripts/m85_mouse_look_check.mjs`. Original failed output and its
source SHA-256 are retained. Human follow-up: move the mouse right/left on the
ground and after F3; confirm the view follows naturally before promoting this
separate correction to main.
