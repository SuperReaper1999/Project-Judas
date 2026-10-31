# Alpha 1.1.0 preparation: camera qualification correction

The first preparation run 41150036125 for ab7988b8 failed Linux native-gates.
The original result is preserved, not waived. Remaining Windows work in that
superseded run was cancelled. No tag, main promotion or publication occurred.

## Confirmed runtime defect

NetworkScriptTests first passed 26 checks then could not load the frozen
Connected Workshop scene. Its error expression was evaluated before the loader
updated the error, producing an empty diagnostic. A direct native runtime probe
reported scene line996: invalid camera projection/pixel settings.

Stage1 added camera lens fields but required them on every camera during decode,
while the default-camera writer deliberately omitted them. Older perspective
records, including the actual frozen networking game, therefore failed. The
reader now preserves the default perspective lens when the entire optional
extension is absent. A present extension still requires complete valid values;
malformed/partial settings reject and preserve the previous loaded scene.

New roundtrip/negative tests cover exactly this missing case, alongside the
existing explicit orthographic camera and real cold-restoration tests. The
networking test now reports the error after the actual load operation.

## Exact matrix test evaluation

The unchanged SpriteMathTests failed after19 checks on Ubuntu24.04/GCC13/GLM0.9.
An instrumented copy of that same test recorded coefficient differences:
(0,0) actual0.866025328636 vs literal reference0.866025447845;
(1,1) actual1.73205065727 vs1.73205089569. Maximum difference2.38418579102e-7.
A standalone simplified probe did not reproduce the literal mismatch, while
both paths using runtime inputs compared bit-identically. Disabling builtins
alone did not fix the original; do not attribute the discrepancy solely to tanf.
The demonstrated issue is differing literal/runtime expression evaluation in
this compiler/GLM test context, not a demonstrated changed camera algorithm.

The authoritative test now compares the same legacy GLM operation with runtime
inputs on both sides, still with exact equality and no epsilon relaxation. A
wrong-FOV negative control must differ. No production projection formula changed.
Ubuntu release-toolchain and current native runs pass21 checks.

## Results

- Original two failures and source/probes retained; neither counted as passing.
- Sprite integration57 checks, including six new default/invalid lens checks.
- Independent cold sprite/lens restore5 checks.
- Network JS37 checks; actual remote diagonal motor speed5.499990940 m/s for the
  existing5.5 m/s contract. No gameplay/controller thresholds were changed.
- Full exact-source release preparation must run again on the corrected commit.

These focused Linux tests ran with the current native source including separately
uncommitted browser #ifdef work. Final clean-source CI, rather than these local
binaries alone, is the release acceptance gate. The subsequent release commit
contains only the four reviewed camera/test files, notes and this evidence;
Browser work remains uncommitted and is not part of Alpha1.1.0.
