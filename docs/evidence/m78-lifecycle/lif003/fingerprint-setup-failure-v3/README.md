# Preserved final-verifier setup failure

The first V3 checker compared the entire identity dictionary with historical
records that additionally contain a `role` field. This reported unchanged files
as mismatches. No byte/SHA mismatch was established. The current/source/output
groups passed; the failed historical group and exact executed checker are retained.

The generic checker now compares only bytes and SHA256, preserving all original
expected hashes and the initial V3 capture/failed receipt. A separate V4 capture
records the final checker and this failure. No engine/test change or rerun.
