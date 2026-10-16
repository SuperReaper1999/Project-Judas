# Evidence packaging follow-up

The first local M91 checkpoint, `e8c0e8064189928c04a2154bb895745af2e5c2e2`,
contained two raw comparison CSV blobs normalized from CRLF to LF by Git. The
index-byte gate detected the difference, but the surrounding shell continued to
the commit despite that gate failing. It was not pushed at that point.

The measured numerical values, physics, tests and working original files were
unchanged. `raw/PACKAGING_LINE_ENDINGS.json` preserves the original/committed hashes
and demonstrates that both differences were exclusively newline normalization.

A normal follow-up commit restores their original bytes and adds an archive-local
`raw/** -text` rule. The original expected experiment hashes are retained. The
verifier additionally checks the actual Git index blobs with `--git`, and the
commit operation is conditional on that gate succeeding. No history is rewritten.
Four redundant compressed screenshot copies were also removed; the original PNGs
remain. No implementation change or numerical validation rerun was needed.
