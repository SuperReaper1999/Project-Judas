# Independent M58 shaping references

Logical UTF-8 samples, exact font SHA256 and command arguments are in
`reference.json`. `reference.h` is their compact C++ representation. Generated
with the **unmodified upstream HarfBuzz 10.4.0 `util/hb-shape.cc`** from the pinned
archive, not Judas's shaping wrapper. OT font functions, scale 1536, ppem 24,
cluster level 0, paragraph BOT/EOT flags. Offsets/advances are 1/64 pixel units.

The [historical verifier at `f40fca70`](https://github.com/SuperReaper1999/Project-Judas/blob/f40fca70f409ee9dbac1bb69b8d6170442fcc947/scripts/m58_reference.py)
can reproduce the fixture comparison from a checkout of that revision with the
pinned upstream `hb-shape` binary. It checks the font hashes and compares
upstream glyph output with the stored JSON. The script leaves both fixture files
untouched. Current Experimental consumes these checked-in references and has
no maintained regeneration command for them.

The focused suite separately verifies UAX#9 visual order and wrapped lines against
ICU's direct visual-index API, CJK/grapheme/shape boundaries and actual GL glyph
positions against a CPU FreeType coverage oracle. These samples establish integration
correctness, not universal Unicode/font or linguistic certification.
