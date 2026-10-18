# Raw Windows receipt line endings

The SDK read-only checkpoint `df8929f73505d407697b5d35b115fc71d9100471`
preserves the original Windows JSON receipts, including their BOM/CRLF bytes.
Git's whitespace check reported those carriage returns. A shell sequencing
mistake allowed the commit after that check returned nonzero; the intended
pre-commit inventory check therefore had not run at that point.

An immediate independent follow-up verified the exact intended **67 committed
paths**, every committed blob against its working file, all evidence manifest
lengths/hashes and absence of generated binaries/caches. All matched; there was
no source, scope or evidence-byte discrepancy. Linux mechanism and real-SDK
checks had already passed before checkpointing.

`STAGING_WHITESPACE.log.gz` retains the reproduced original Git diagnostic.
A scoped `WINDOWS_ORIGINAL_*.json -whitespace` attribute now distinguishes these
raw receipts from editable source. Receipt bytes are unchanged; only their
attribute entry and its evidence-manifest hash change. No history amendment or
force push is used. Final staging checks run as separately checked steps.

Final staging independently caught trailing spaces emitted by original Linux
application logs. `FINAL_STAGE_WHITESPACE.log.gz` preserves that diagnostic.
A `logs/** -whitespace` attribute preserves those raw bytes; its manifest is
updated. The guarded staging step stopped before commit, as intended. Source
files remain subject to the normal whitespace check.
