# Real SDK workflow runner corrections

The source-e272a114 SDK passed all 45 native cases and seven extra physics suites,
42 native script checks and actual VM enumeration of all 477 public API members.
The isolated SDK editor created a project, saved and reopened it and ran Play/Stop.

Two sequential actual tool runs exposed mistakes in the new qualification runner:
- The environment bake positive input 8/8 was outside supported width/sample
  limits. It correctly failed; the positive case now uses 16/32 and the original
  8/8 invocation is retained as an explicit negative control.
- The frozen mod project omitted Imports/landscape.judasterrain and NOTICE.txt,
  which its own curated contract explicitly publishes. These exact tracked game
  files are now included. The mod kit continues to require its entire declared
  source closure; the actual game and engine authoring policy are unchanged.

The corrected development run passes all SDK operations and two independently
relocated standalone exports in package-only namespaces. This receipt applies to
its named e272a114 binaries and corrected runner, not a later source identity.
The final exact-source rerun and Windows results accompany the final artifacts.
Original failures are retained, rather than converted into ceremonial passes.
