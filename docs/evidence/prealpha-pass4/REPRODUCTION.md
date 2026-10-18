# Pass 4 reproduction and evidence identity

Start from the checkpoint named by each receipt. A refactor-only comparison is
not the same source as the later scheduling optimization. Do not run the pure
extraction checker against the optimized implementation and interpret its
expected changed-body detection as a physics regression.

| Stage | Source / reproduction |
|---|---|
| Accepted physical baseline | `7da56c2cc6ce7acda827a6e4019d610622b237e2` |
| Extraction / formatting only | `06318379b5dfc99fe54eaedebbe5e6d964b25de8`; [independent proof and commands](refactor/REPORT.md), `refactor/reproduction/` |
| Exact scheduling changes | `b4305b790ee2d4a6b8cfbef31b449ae50d4e03b6`; [matched measurements](dense/REPORT.md), `dense/reproduction/` |
| Preserved CCD/liquid experiments | `60a92176adbdfb8ccf90cc1fe3a40505416b1559`; [CCD](ccd/REPORT.md), [liquid](liquid-query/REPORT.md) |
| Final implementation / MSVC correction | `9deb47d1667e5b6bd1abe708ed343c3fbf6fe847`; [exact module-byte proof](platform/windows-string-split/README.md), [final Linux checks](linux-final-portability/REPORT.md) |
| Packaged Python inspection fix | `df8929f73505d407697b5d35b115fc71d9100471`; [original failure and strict controls](sdk-readonly/REPORT.md), [real Linux SDK](sdk-readonly/linux-native/REPORT.md); no native C++ change |

The raw `RESULTS.json` files retain argv, environment, working directory,
timestamps, expected/actual exits and executable/log hashes. Use fresh owned
output directories. Never overwrite the original failure or old save snapshot.
Cache-only research observers are not production features and their instrumented
timings do not replace the uninstrumented comparison.

The Linux qualification runner and narrow follow-up are retained under
`linux/reproduction/` and `linux-final-portability/reproduction/`. Their reports
explain the matching checkout/cache layout and paired helper import. They build
only selected targets. No full-suite run is required to reproduce this pass.

For the final implementation, verify source and all per-directory evidence
manifests from the repository root without compiling anything:

```sh
python3 - <<'PY'
import hashlib, json
from pathlib import Path
root = Path('docs/evidence/prealpha-pass4')
source = json.loads((root / 'FINAL_FINGERPRINTS.json').read_text())
for item in source['source_files'] + source['tooling_files']:
    data = Path(item['path']).read_bytes()
    assert len(data) == item['bytes'], item['path']
    assert hashlib.sha256(data).hexdigest() == item['sha256'], item['path']
count = 0
for manifest in root.rglob('MANIFEST.json'):
    for item in json.loads(manifest.read_text())['files']:
        data = (manifest.parent / item['path']).read_bytes()
        assert len(data) == item['bytes'], item['path']
        assert hashlib.sha256(data).hexdigest() == item['sha256'], item['path']
        count += 1
print('Source fingerprints and', count, 'manifest entries match.')
PY
```

The final document/evidence checkpoint can follow the qualified implementation
without recompiling identical C++ merely to embed the report commit. Binary
provenance remains the **actual** build identity: the Linux original/follow-up
receipts distinguish clean `60a9217` from the then-dirty literal candidate; the
source hashes bind that candidate to committed `9deb47d1`. The complete GCC engine
archive is byte-identical across those builds. No falsely clean later revision
is substituted into a binary receipt.

Portable source archives, SDKs and game packages are intentionally outside Git.
Their checksums and manifests remain in the receipts. A successful software-GL
or VM application test does not imply physical hardware acceptance, and the
unchanged failed numerical contracts remain failed.
