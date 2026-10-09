# Gravity selection lab

Ordinary project for the optional per-entity gravity API. Engine defaults are unchanged.

WASD / mouse or controller sticks: move/look. Space: jump. **G**: aim at a wall or ceiling and make that surface your floor. **F**: restore ordinary spatial gravity. **C**: select the rotated uniform source outside its region. **T**: select RadicalGravity toward the remote source. **R**: reload; Escape: pause/resume.

Blue prop falls under ordinary spatial gravity. Gold prop rises under its own authored uniform selection. Neither changes when the character switches gravity. Selecting gravity preserves momentum; wall impacts remain normal collisions. Only the character and its project-controlled camera reorient. The world is not rotated.

Selection does not grant attachment or alter support normals. This compact lab is an API demonstration, not a finished gravity-switching game. See ../../docs/GRAVITY_SELECTION.md. DejaVu font license is in Assets/fonts/LICENSE.txt.
