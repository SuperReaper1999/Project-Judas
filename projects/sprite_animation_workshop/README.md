# Sprite Animation Workshop — Stage 3

Open `Sprite_Animation_Workshop.judasproj`. WASD/arrows move, Space jumps,
E plays an interaction whose strike marker applies a real impulse to a nearby
physical object. 1/2/3 switch platformer/top-down/rotated scenes; R restarts;
F6/F7 save/load. Idle/run/jump/interaction selection is ordinary project JS.

Right-click `traveller.png` → Create/edit sprite sheet, or double-click
`traveller.judassprite`. Slice cells 24×48. Select named frames, edit clips,
variable durations and event markers. Preview/scrub is isolated from gameplay.
Save the asset explicitly; reopen Play to inspect changes. The sheet is project
content and its PNG is an explicit export dependency. No skeleton is fabricated.

Source art and reproducible generation: `tools/demos/generate_sprite_animation_lab.py`.
This is Stage 3 content, not a release or a human-acceptance claim.
