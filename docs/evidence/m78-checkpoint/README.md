# M78 corrective checkpoint byte preservation

This checkpoint preserves the exact combined candidate recorded by the immutable
[closure manifest](../m78-closure/FINAL_FINGERPRINTS.json). It is a corrective
checkpoint, not comprehensive M78 completion or human validation.

During staging, the repository-wide `text=auto` rule normalized CRLF native
Windows evidence. The staged bytes therefore failed the recorded SHA256 checks.
No original source, evidence, expected hash or save fixture was changed. A narrow
root `.gitattributes` rule now marks `docs/evidence/m78*/**` as `-text`, preserving
raw evidence bytes in Git and subsequent checkouts. All candidate paths were
restaged and compared with both working-file bytes and recorded fingerprints.

The original 4,855 file / 94 binary closure verification is preserved. Only the
byte-preservation attribute and this new checkpoint account/verification receipt
are additions to its intended checkpoint inventory. MIT terms remain unchanged;
the authorized README/LICENSE owner is Conner/SuperReaper1999.

The verification receipt explicitly excludes its own self-hash. No build/test
rerun, commit amendment, force push, tag or M79 work accompanies this hygiene fix.
