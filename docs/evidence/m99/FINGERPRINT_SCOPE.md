# Final candidate fingerprints

SOURCE_SHA256.txt covers every changed implementation, test, tool, current
reference/type and game asset outside the evidence directory. Paths are repository
relative. EVIDENCE_SHA256.txt covers the evidence files, including source/binary
manifests and the complete changed-file inventory, except its own digest.

BINARY_SHA256.json records the exact local Release executables, APK, archive and
Android native build products. These binaries are not committed. Android BUILD.json
records unstripped native libraries; APK_NATIVE.json records the stripped entries
actually packaged. Their different hashes are intentional.

The candidate is identified by its base commit plus these exact source bytes.
The delivery response supplies its eventual Experimental commit hash. Hash identity
is provenance evidence, not a substitute for the behavioural checks.
