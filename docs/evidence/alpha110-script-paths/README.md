# Alpha 1.1.0 authoring path qualification

RC2 `878a6459da5abab8817cdf14ca1d7796cbc8ce61` passed Linux release preparation
but failed the newly included native Windows script-authoring case in run
41152687392. Its original output omitted the underlying error. Diagnostic
checkpoint f1d9ec9c reports that error after the call; it does not change behavior.
The compressed original log and its uncompressed hash are retained here.

Independent Linux reproduction: run the existing workflow binary with TMPDIR
pointing to a symlinked directory. Script creation canonicalized the destination,
while AssetDatabase retained its lexical root, so Track rejected the new file as
outside Assets. The first correction exposed the identical mistake in generated
capsule/cylinder assets during player-preset creation; that failure is retained.

The correction validates physical ownership with canonical paths, then creates
and registers through the asset database's own absolute normalized path spelling.
The registry's ownership rules, sidecar handling, no-overwrite behavior and script
path traversal guards remain intact. No global path-comparison relaxation.

The normal workflow passes 70 checks on Linux. The complete workflow also passes
70 checks with an aliased TMPDIR, covering actual registration, player presets,
source compilation, runtime execution, Undo/Redo and the inspector create dialog.
The test independently opens a project through a directory symlink and rejects a
foreign project before writing. On Windows the existing real temp path is used;
Unix-only symlink creation does not claim Windows symlink permissions.

Native Windows diagnostic/repaired exact-source release preparation must still
complete before publication. These focused local binaries also contain separate,
uncommitted browser conditional code; clean-source release CI is authoritative.
