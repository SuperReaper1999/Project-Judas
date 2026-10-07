# M68 development failures and follow-ups

These records are preserved development evidence, not final passing results.
No historical FTFT/P1 or earlier milestone report was rewritten.

| Original observation | Cause | Correction / affected follow-up |
|---|---|---|
| Baseline first-use harness rejected `REALTIME` without an argument | Incorrect capture command | Retained initial command; corrected to `REALTIME 720`, then desktop three-step reference captured separately |
| Resource build failed on `std::filesystem::file_size` / `std::error_code` | Missing standard includes | Added filesystem/system_error includes; focused build and resource checks |
| New pipeline test failed to compile `Project::Serialize` | Test used a nonexistent native helper | Use actual `SerializeToString`; no public API added |
| Pipeline-1 expected identical cooked bytes after restoring reformatted source JSON | Invalid test expectation: authored provenance hashes source bytes | Restore exact original source bytes; same-input product identity then passes |
| Pipeline-2 attempted to save to an empty scene path | New project deliberately has no inferred startup scene | Test authors `Scenes/start.judas` explicitly; no engine scene guessing |
| Exploratory build named nonexistent async/resource targets | Stale target names | Discover actual `judas_job_tests` and `judas_async_application_tests`; do not call usage/target errors algorithm failures |
| Exploratory scene-author `--help` was treated as a positional output path | Existing CLI positional contract | Use no-argument usage output; no M68 CLI feature expansion |

Original logs and corrected pipeline/reimport/private-upload records are under
[development](development/). The final clean gate and any late affected corrections
are recorded separately. The pre-gate measurement script is preserved as executed;
its later runner guard accepts importer exit 2 only with a successful JSON import
warning, rather than accepting exit 2 from an unrelated command.

Candidate captures predate the final small cleanup of encoded image buffers on
standalone material installation and final test-report fields. These do not change
runtime bookkeeping/import/export/editor semantics measured there. Final clean
resource checks and repeated final-byte captures establish the resource result;
the earlier binaries are not relabelled as the final build.

## Single candidate gate and narrow follow-up

The clean build had zero warnings. Async passed 12 cases / 246 checks. Of 149
discovered production suites, 148 passed; the new pipeline suite failed at its
external-source glTF fixture. The original gate's `overall_pass: false` remains in
[final/RESULTS.json](final/RESULTS.json); it has not been rewritten as passing.

The fixture incorrectly attempted to export an uncooked external-source glTF.
The existing runtime parses self-contained glTF/GLB bytes; M66 owns approved
external source import and cooking. The test now proves self-contained closure,
explicit uncooked-source rejection and last-good package retention. Investigation
also found argument-evaluation order in M68's new dependency diagnostic could
build its message before the collector populated its error. That expression is
now sequenced; collection/export semantics did not change.

The affected pipeline rerun passed **266 checks**, including all 200 history
revisions. Only affected targets were rebuilt; the entire production set was not
repeated. Earlier broad results remain applicable to unchanged mechanisms; their
original source fingerprints remain preserved. Final fingerprints separately
identify the diagnostic/test follow-up. The integration runner also explicitly
isolates imported scenarios' XDG user data before their first execution.

The first moved-package Workshop read then rejected its source save. M61 hashes
serialized project settings; M68 had added packaging-only fields to that serializer.
Normalization of those fields to legacy defaults fixes that genuine M68 regression.
The existing dependency report also retains excluded ID/type/hash records, allowing
the same project-content formula after byte pruning. Actual packaged files remain
hashed rather than trusting their report entries. An independent legacy-formula
check, corruption/edit negatives, affected save checks and moved fresh-process
load were required for this correction and now pass. Save/canonical schemas remain
unchanged. The final pipeline passes **276 checks** and the second full integration
passes all current consumer/editor/export checks, including the moved source-save
read. The first failure logs remain under `followup/integration-original-failure`.

The initial attempt to run live API checking named the wrong cookbook output;
no enumeration was claimed from that missing file. Production discovery's
`judas_..._tests` pattern excludes `judasjs_examples_tests`, so the cookbook/live
check is run explicitly once with its actual output. A missing `<fstream>` include
in the save helper produced a compiler error and was corrected before testing.

The first new save-negative test deliberately corrupted the package report, then
incorrectly used that corrupted report as its repeat-export expected result. The
test now freezes the accepted report before corruption; deterministic rebuilding
and previous-package failure preservation pass. The failed 52-check run remains
under `followup/pipeline-corruption-expectation`.

The extra fresh-process save read initially failed a raw liquid-byte comparison.
Independent decoding showed only owner order differed: captured `4001,4000` versus
restored `4000,4001`. Every owner's entire serialized record and the accounting,
connections and parcel tail were exact. The preserved pre-save-helper binary also
failed that raw-order assertion. Asynchronous resource registration order is not
the stable owner identity contract. The assertion now sorts complete record bytes
by entity ID and still compares every field and the complete tail. The final read
passes 20 checks; **no production liquid or persistence representation changed**.
Raw failures and the independent comparison are retained under `followup/save`.
The temporary diagnostic needed the tests include directory when compiled outside
its normal CMake source folder; that command error did not alter repository code.

Review of actual consumer observations showed the inherited street scenario's T
key exercised checkpoint, not bail. M68 does not relabel it as ragdoll proof. A
narrow `--bails-only` application run uses the authored logical action (X), proves
two independent enter/recovery cycles with 13 valid mapped bodies, and retains its
state records under `followup/bails`. Original consumer scripts/evidence stay intact.

`api-types-live-final.log` records the corrected live-enumeration pass; the initial
missing-output log is preserved. There was no repeated full production run.
