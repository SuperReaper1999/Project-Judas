# Initial fingerprint verifier setup failure

The first final capture remains byte-for-byte at `../FINAL_FINGERPRINTS.json`
and `../CHANGED_FILES.txt`. It is an initial capture, not a passing handoff.
The exact executed verifier and its nonzero result are preserved here.

All 659 current file hashes, 4,850 unchanged previous file hashes, 78 current
local outputs, 94 previous local outputs and excluded-artifact hashes matched.
The inventory comparison incorrectly excluded every file whose basename was
`FINAL_VERIFICATION.json`, including the nested native receipt that was correctly
present in the hashed inventory. There was no source/binary/evidence mismatch.

The generic verifier correction compares full paths and excludes only its own
top-level metadata. The subsequent final capture uses separate `_V2` metadata;
the initial expected values are not replaced. Runtime and test sources, their
successful receipts and the frozen native evidence are unchanged.
