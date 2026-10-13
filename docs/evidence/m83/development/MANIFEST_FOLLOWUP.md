# New lab world-manifest authoring failure

`application-manifest-first.txt` preserves the failed application write run.
`manifest-first/` retains its exact world manifest and controller. Annex lookup
made the malformed manifest reachable at normal startup rather than only on F3.

The region row's `35 3 0` values are half extents, not a centre position. Its zero
Z half extent correctly failed the existing generic parser's strictly-positive
bounds requirement. The current ordinary content uses `40 4 6`, covering the
annex's authored stand and floor. No parser, runtime mechanism, quality or test
condition changed. The final write/restore and retained-region runs test this
corrected manifest through the ordinary project path.
