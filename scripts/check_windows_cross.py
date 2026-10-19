#!/usr/bin/env python3
"""Focused non-executing cross-export contracts against a real generated PE.

Requires an actual MinGW build (no synthetic PE substitutes). No Windows/Wine is
launched. Native Windows functional acceptance is a separate required receipt.
"""
import argparse
import copy
import json
from pathlib import Path
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import windows_runtime as runtime
import export_windows as exporter


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, out = args.build.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    rows = []

    def check(name, function, rejects=False):
        try:
            function()
            passed, detail = not rejects, 'accepted'
        except (ValueError, OSError, KeyError, UnicodeError) as error:
            passed, detail = rejects, str(error)
        rows.append({'name': name, 'passed': passed, 'expectedRejection': rejects, 'detail': detail})
        print(('PASS ' if passed else 'FAIL ') + name)

    raw = (build / 'judas.exe').read_bytes()
    actual = runtime.embedded_identity(raw)
    check('real PE embedded identity matches CMake target identity',
          lambda: assert_equal(actual, json.loads((build / 'judas-build-info-Release.json').read_text())))
    check('truncated PE is rejected', lambda: runtime.embedded_identity(raw[:64]), True)
    corrupt = bytearray(raw)
    corrupt[0:2] = b'NO'
    check('wrong binary format rejected', lambda: runtime.embedded_identity(bytes(corrupt)), True)
    check('empty embedded metadata rejected', lambda: runtime.embedded_identity(b''), True)
    text = json.dumps(actual).encode()
    check('sidecar JSON is not treated as binary provenance', lambda: runtime.embedded_identity(text), True)
    with tempfile.TemporaryDirectory(prefix='judas-cross-contracts-') as temporary:
        root = Path(temporary)
        bundle = root / 'bundle'
        manifest = runtime.prepare(build, bundle)
        check('real runtime bundle verifies', lambda: runtime.verify(bundle))
        check('bundle rejects replacing an existing directory', lambda: runtime.prepare(build, bundle), True)
        path = bundle / 'judas.exe'
        original = path.read_bytes()
        path.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
        check('changed executable bytes rejected', lambda: runtime.verify(bundle), True)
        path.write_bytes(original)
        metadata = bundle / runtime.MANIFEST
        changed = copy.deepcopy(manifest)
        changed['build']['compiler_version'] = 'invented'
        metadata.write_text(json.dumps(changed))
        check('forged sidecar identity rejected against embedded resource', lambda: runtime.verify(bundle), True)
        metadata.write_text(json.dumps(manifest))
        dll = next(bundle.glob('*.dll'))
        held = dll.read_bytes()
        dll.unlink()
        check('missing runtime DLL rejected', lambda: runtime.verify(bundle), True)
        dll.write_bytes(held)
        extra = bundle / 'unexpected.dll'
        extra.write_bytes(held)
        check('unexpected runtime DLL rejected', lambda: runtime.verify(bundle), True)
        extra.unlink()
        check('positive bundle remains valid after negative controls', lambda: runtime.verify(bundle))
        host = dict(actual, platform='Linux')
        check('same-source cross-platform pair accepted', lambda: exporter.compatible(host, actual))
        for field, value in [('source_fingerprint', '0' * 64), ('source_revision', '1' * 40),
                             ('channel', 'main' if host['channel'] != 'main' else 'Experimental'),
                             ('source_untracked_count', host['source_untracked_count'] + 1)]:
            other = dict(host)
            other[field] = value
            check('mismatched ' + field + ' rejected', lambda other=other: exporter.compatible(other, actual), True)
        paths = root / 'paths'
        paths.mkdir()
        for name in ['RUNTIME_REQUIREMENTS.txt', 'BUILD_PROVENANCE.json', 'DEPENDENCIES.json', 'judas-package.txt']:
            (paths / name).write_text('native generated content')
        check('existing native exporter reserved names remain valid', lambda: exporter.windows_paths(paths))
        for name in ['CON.txt', 'trailing.', 'trailing ', 'bad:name', 'judas.exe', 'kernel32.dll', 'ENGINE']:
            p = paths / name
            p.write_text('negative')
            check('Windows path rejects ' + repr(name), lambda: exporter.windows_paths(paths), True)
            p.unlink()
        (paths / 'Twin').write_text('a')
        (paths / 'twin').write_text('b')
        check('case-insensitive alias rejected', lambda: exporter.windows_paths(paths), True)
    result = {'schema': 1, 'scope': 'metadata, native-target closure and cross-export negative contracts; no Windows execution',
              'runtime': actual, 'checks': len(rows), 'failures': sum(not x['passed'] for x in rows), 'results': rows}
    (out / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
    return 1 if result['failures'] else 0


def assert_equal(a, b):
    if a != b:
        raise ValueError('Values differ')


if __name__ == '__main__':
    raise SystemExit(main())
