# Initial untracked inventory clarification

`UNRELATED_UNTRACKED_PRESERVED.txt` is the raw initial file listing. It was
captured after the new M82 `src/Powertrain.h` had been created, so that one entry
is milestone implementation, not unrelated operator work. It is intentionally
included in the M82 commit and source fingerprints. The original listing is
retained unchanged rather than concealing the bookkeeping error.

All 74 actual unrelated initial entries still exist. Git's default status may
collapse their individual Python cache files into directory entries. No tracked
historical evidence, existing project, independent research or fluid file was
modified. Final Git status has no tracked changes; unrelated untracked files
and caches remain outside the checkpoint.

This clarification changes documentation/evidence only. The tested M82 source
and binaries are unchanged from implementation commit `23b75969`.
