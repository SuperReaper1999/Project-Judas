#!/usr/bin/env python3
"""Export a native Windows game on Linux using a checked cross-runtime bundle.

The existing native exporter performs asset/cook/script/dependency validation in
an owned temporary package. This second target-assembly phase replaces only the
native runtime payload, adds the verified Windows closure and publishes once.
No Windows executable is run on Linux; Wine and a Windows SDK are not required.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from package_sdk import probe_identity
from windows_runtime import MANIFEST as RUNTIME_MANIFEST, digest, embedded_identity, file_record, closure, verify as verify_runtime

RESERVED = {'judas', 'judas.exe', 'engine', 'game.judasproj', 'judas-package.txt',
            'runtime_requirements.txt', 'game-icon.png', 'dependencies.json',
            'build_provenance.json', 'windows_package.json', 'sha256sums'}
DEVICES = re.compile(r'^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\..*)?$', re.I)
NATIVE_RESERVED = {'judas', 'engine', 'game.judasproj', 'judas-package.txt',
                   'RUNTIME_REQUIREMENTS.txt', 'game-icon.png', 'DEPENDENCIES.json', 'BUILD_PROVENANCE.json'}

def records(root):
    return [file_record(root, path) for path in sorted(root.rglob('*'))
            if path.is_file() and path.relative_to(root).as_posix() != 'WINDOWS_PACKAGE.json']

SOURCE_FIELDS = ('version', 'maturity', 'channel', 'source_revision', 'source_tree',
                 'source_fingerprint', 'source_dirty', 'source_origin',
                 'source_untracked_count', 'source_untracked_fingerprint')


def compatible(host, target):
    for field in SOURCE_FIELDS:
        if host.get(field) != target.get(field):
            raise ValueError('Host exporter / Windows runtime differ in ' + field + '; rebuild both from the same source')
    if host.get('platform') != 'Linux' or host.get('configuration') != 'Release':
        raise ValueError('Cross export requires a native Linux Release exporter')


def windows_paths(root):
    seen = set()
    for path in root.rglob('*'):
        if path.is_symlink():
            raise ValueError('Symlink is not a portable Windows package entry: ' + str(path))
        rel = path.relative_to(root)
        key = rel.as_posix().casefold()
        if key in seen:
            raise ValueError('Case-insensitive Windows path collision: ' + str(rel))
        seen.add(key)
        # Windows normalizes trailing dots/spaces and device names independently
        # of extension; checking on Linux is essential before packaging.
        for part in rel.parts:
            if part.endswith((' ', '.')) or any(c in part for c in '<>:"\\|?*') or any(ord(c) < 32 for c in part) or DEVICES.fullmatch(part):
                raise ValueError('Invalid Windows package path: ' + str(rel))
        if rel.parts[0].casefold().endswith('.dll'):
            raise ValueError('Project content collides with Windows runtime DLL namespace: ' + str(rel))
        if rel.parts[0].casefold() in RESERVED and rel.parts[0] not in NATIVE_RESERVED:
            raise ValueError('Project content collides with reserved Windows path: ' + str(rel))


def export(project, output, bundle, exporter, native_runtime, engine_data=None):
    project, output, bundle = Path(project).resolve(), Path(output).absolute(), Path(bundle).resolve()
    exporter, native_runtime = Path(exporter).resolve(), Path(native_runtime).resolve()
    if output.exists() or output.is_symlink():
        raise ValueError('Choose a new package destination; existing content is never overwritten')
    if output == project.parent or output.is_relative_to(project.parent) or project.parent.is_relative_to(output):
        raise ValueError('Package must be outside, and must not contain, the source project')
    target = verify_runtime(bundle)
    host = probe_identity(exporter)
    compatible(host, target['build'])
    output.parent.mkdir(parents=True, exist_ok=True)
    workspace = Path(tempfile.mkdtemp(prefix='.judas-cross-export-', dir=output.parent))
    package = workspace / 'content'
    try:
        env = os.environ.copy()
        if engine_data:
            env['JUDAS_ENGINE_ROOT'] = str(Path(engine_data).resolve())
        result = subprocess.run([str(exporter), str(project), str(package), str(native_runtime)],
                                env=env, text=True, capture_output=True, timeout=300)
        if result.returncode:
            raise ValueError('Native asset export failed: ' + result.stdout + result.stderr)
        windows_paths(package)
        provenance = json.loads((package / 'BUILD_PROVENANCE.json').read_text())
        if provenance['exporter'] != host:
            raise ValueError('Asset-export provenance differs from checked host exporter')
        (package / 'judas').unlink()
        for row in target['files']:
            source = bundle / row['path']
            if row['path'].startswith('licenses/'):
                destination = package / 'engine' / row['path']
            else:
                destination = package / row['path']
            if destination.exists():
                raise ValueError('Windows payload would overwrite content: ' + row['path'])
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        # Verify after copy as well: the bundle must not have changed between
        # metadata inspection and publication. Every target byte is checked.
        for row in target['files']:
            destination = package / ('engine/' + row['path'] if row['path'].startswith('licenses/') else row['path'])
            if digest(destination) != row['sha256']:
                raise ValueError('Runtime payload changed during export: ' + row['path'])
        if embedded_identity((package / 'judas.exe').read_bytes()) != target['build']:
            raise ValueError('Packaged PE identity differs from verified bundle')
        provenance.update(runtime=target['build'], runtime_sha256=digest(package / 'judas.exe'),
                          target='Windows-x64', runtime_bundle_sha256=digest(bundle / RUNTIME_MANIFEST))
        (package / 'BUILD_PROVENANCE.json').write_text(json.dumps(provenance, indent=2) + '\n')
        (package / 'RUNTIME_REQUIREMENTS.txt').write_text(
            'Native Windows x64 Judas game. Run judas.exe on Windows 10/11 with an OpenGL 3.3 driver.\n'
            'Cross-built on Linux with MinGW-w64. No Visual Studio, compiler, Wine, Linux host or VM is needed to play.\n'
            'SDL2, ICU Unicode data/text dependencies and engine libraries are statically linked.\n'
            'Required non-system compiler runtime DLLs are bundled beside judas.exe. Do not remove them.\n'
            'Windows system APIs and a functioning graphics/audio device remain operating-system requirements.\n'
            'Saves use LocalAppData/judas/games/<save-id>/Saves, not the installation directory.\n'
            'Notices: engine/licenses and engine/third_party/RUNTIME_NOTICES.txt.\n'
            'Checksums: WINDOWS_PACKAGE.json. Metadata verification is not hardware/gameplay acceptance.\n')
        files = records(package)
        report = {'schema': 1, 'kind': 'judas-windows-game', 'runtime': target['build'],
                  'hostExporter': host, 'systemImports': target['systemImports'], 'files': files,
                  'qualification': 'Linux export complete; native Windows/device acceptance is separately recorded'}
        (package / 'WINDOWS_PACKAGE.json').write_text(json.dumps(report, indent=2) + '\n')
        verify_package(package)
        package.rename(output)
        return report
    finally:
        shutil.rmtree(workspace)


def verify_package(root):
    root = Path(root).resolve()
    value = json.loads((root / 'WINDOWS_PACKAGE.json').read_text())
    if value.get('schema') != 1 or value.get('kind') != 'judas-windows-game':
        raise ValueError('Invalid Windows package manifest')
    if any(path.is_symlink() for path in root.rglob('*')):
        raise ValueError('Package symlinks are not supported')
    actual = records(root)
    if actual != value['files']:
        raise ValueError('Windows package inventory/checksum mismatch')
    if embedded_identity((root / 'judas.exe').read_bytes()) != value['runtime']:
        raise ValueError('Actual Windows runtime identity mismatch')
    if closure(root) != value['systemImports']:
        raise ValueError('Windows package import closure mismatch')
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--runtime-bundle', type=Path)
    parser.add_argument('--host-exporter', type=Path)
    parser.add_argument('--host-runtime', type=Path)
    parser.add_argument('--engine-data', type=Path)
    parser.add_argument('--verify', type=Path, help='Verify an existing package; do not execute it')
    args = parser.parse_args()
    try:
        if args.verify:
            value = verify_package(args.verify)
        else:
            if not all((args.project, args.output, args.runtime_bundle, args.host_exporter, args.host_runtime)):
                parser.error('Export requires --project, --output, --runtime-bundle, --host-exporter and --host-runtime')
            value = export(args.project, args.output, args.runtime_bundle, args.host_exporter,
                           args.host_runtime, args.engine_data)
        print(json.dumps({'files': len(value['files']), 'runtime': value['runtime']}, indent=2))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, 'Windows export failed: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
