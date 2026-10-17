#!/usr/bin/env python3
"""Prepare a hashed, exact-source native Windows functional fixture; does not start a VM."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FILES = ["engine-version.json", "src/BuildIdentity.cpp", "src/BuildIdentity.h",
         "src/BuildUpdate.cpp", "src/BuildUpdate.h", "src/ToolProcess.cpp", "src/ToolProcess.h",
         "src/ProcessArguments.h",
         "src/NetworkTransport.cpp", "src/NetworkTransport.h", "tests/BuildIdentityTests.cpp",
         "tests/NetworkTransportTests.cpp", "third_party/nlohmann/json.hpp",
         "cmake/JudasBuildIdentity.cmake", "cmake/JudasBuildIdentity.h.in",
         "cmake/JudasBuildIdentity.json.in", "cmake/JudasSourceInfo.json.in"]


def prepare(output, replace=False):
    if output.is_symlink() or output.with_suffix('.json').is_symlink():
        raise ValueError('Fixture outputs must not be symlinks')
    output = output.resolve()
    if output.suffix.lower() != '.zip':
        raise ValueError('Prepared Windows fixture output must end in .zip')
    if ROOT in output.parents and ROOT / '.cache' not in output.parents:
        raise ValueError('Use .cache or a directory outside the source checkout for generated Windows fixtures')
    paths = [output, output.with_suffix('.json')]
    if any(path.is_symlink() for path in paths):
        raise ValueError('Fixture outputs must not be symlinks')
    if any(path.exists() for path in paths) and not replace:
        raise ValueError('Fixture or receipt exists; use --replace for these generated outputs')
    content = {name: (ROOT / name).read_bytes() for name in FILES}
    content["CMakeLists.txt"] = (ROOT / "packaging/windows/PreAlphaChecks.CMakeLists.txt").read_bytes()
    content["Run-PreAlphaChecks.ps1"] = (ROOT / "packaging/windows/Run-PreAlphaChecks.ps1").read_bytes()
    records = [{"path": name, "sha256": hashlib.sha256(data).hexdigest()} for name, data in sorted(content.items())]
    manifest = ('{"schema":1,"files":[\n' + ',\n'.join(json.dumps(row, separators=(',', ':')) for row in records) + '\n]}\n').encode()
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    tree = subprocess.check_output(['git', 'rev-parse', 'HEAD^{tree}'], cwd=ROOT, text=True).strip()
    info = {'schema': 1, 'revision': revision, 'tree': tree, 'source_dirty': True,
            'source_fingerprint': hashlib.sha256(manifest).hexdigest(), 'channel': 'Experimental',
            'source_files_sha256': hashlib.sha256(manifest).hexdigest(),
            'selection': 'dependency-light-functional-fixture-with-custom-CMake-harness'}
    content['JUDAS_SOURCE_FILES.json'] = manifest
    content['JUDAS_SOURCE_INFO.json'] = (json.dumps(info, sort_keys=True, indent=2) + '\n').encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(content.items()):
            entry = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)
    receipt = {'schema': 1, 'scope': info['selection'], 'source': info, 'files': records,
               'archiveSha256': hashlib.sha256(output.read_bytes()).hexdigest(), 'bytes': output.stat().st_size}
    output.with_suffix('.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({key: receipt[key] for key in ('scope', 'archiveSha256', 'bytes')}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--replace', action='store_true')
    args = parser.parse_args()
    try:
        prepare(args.output.absolute(), replace=args.replace)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Windows fixture preparation failed: {error}\n')


if __name__ == '__main__':
    main()
