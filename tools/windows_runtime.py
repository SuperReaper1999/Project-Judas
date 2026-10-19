#!/usr/bin/env python3
"""Prepare/verify a native Windows runtime bundle without executing a PE file.

Reads embedded RCDATA build identity, checks PE machine/import closure and hashes.
Manifests detect accidental substitution/corruption, not malicious replacement.
Windows API compatibility and gameplay must still be tested on genuine Windows.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import tempfile

sys.dont_write_bytecode = True
from binary_dependencies import Reader, pe
from package_sdk import validate_identity

MANIFEST = 'WINDOWS_RUNTIME.json'
# These are OS contracts, never copied from the build host. The Windows
# qualification additionally resolves actual System32 files/imports.
SYSTEM_DLLS = set(('advapi32 avrt bcrypt cfgmgr32 comctl32 comdlg32 crypt32 d3d9 '
                  'd3d11 dinput8 dwmapi dxgi dxguid gdi32 hid imm32 iphlpapi kernel32 '
                  'ksuser mf mfplat mfreadwrite mfuuid msacm32 msvcrt ntdll ole32 '
                  'oleaut32 opengl32 powrprof propsys psapi rpcrt4 secur32 setupapi '
                  'shell32 shlwapi user32 userenv uuid uxtheme version winmm winspool '
                  'wintrust ws2_32 wtsapi32 xinput1_4 xinput9_1_0 ucrtbase').split())
SYSTEM_DLLS = {name + '.dll' for name in SYSTEM_DLLS}


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def system_dll(name):
    return name.lower() in SYSTEM_DLLS or bool(re.fullmatch(r'(api|ext)-ms-[a-z0-9-]+\.dll', name.lower()))


def embedded_identity(data):
    """Read our unique numeric RCDATA 101 resource through bounded PE tables."""
    metadata = pe(data)
    if metadata['machine'] != 0x8664 or metadata['format'] != 'PE32+':
        raise ValueError('Windows runtime must be a native x64 PE32+ binary')
    r = Reader(data)
    base = r.unpack('<I', 0x3c)[0]
    count, optional_size = r.unpack('<H', base + 6)[0], r.unpack('<H', base + 20)[0]
    optional = base + 24
    if optional_size < 136 or r.unpack('<I', optional + 108)[0] < 3:
        raise ValueError('PE has no declared resource directory')
    resource, resource_size = r.unpack('<II', optional + 112 + 2 * 8)
    if not resource or not 16 <= resource_size <= 32 * 1024 * 1024:
        raise ValueError('Runtime has no bounded embedded Judas build resource')
    sections = [r.unpack('<8sIIIIIIHHI', optional + optional_size + i * 40)
                for i in range(count)]

    def position(address, length):
        for section in sections:
            _, _, virtual, size, raw, *_ = section
            if virtual <= address and address + length <= virtual + size:
                r.block(raw + address - virtual, length)
                return raw + address - virtual
        raise ValueError('Resource RVA is not backed by a file section')

    def entries(offset):
        if offset > resource_size - 16:
            raise ValueError('Resource directory outside declared extent')
        at = position(resource + offset, 16)
        named, ids = r.unpack('<HH', at + 12)
        total = named + ids
        if total > 4096 or offset + 16 + total * 8 > resource_size:
            raise ValueError('Invalid resource directory size')
        return [r.unpack('<II', position(resource + offset + 16 + i * 8, 8))
                for i in range(total)]

    offset = 0
    for expected in (10, 101):
        matching = [child for identifier, child in entries(offset) if identifier == expected]
        if len(matching) != 1 or not matching[0] & 0x80000000:
            raise ValueError('Missing/duplicate Judas build resource directory')
        offset = matching[0] & 0x7fffffff
    languages = entries(offset)
    if len(languages) != 1 or languages[0][1] & 0x80000000:
        raise ValueError('Ambiguous/cyclic Judas build resource language')
    leaf = languages[0][1]
    if leaf + 16 > resource_size:
        raise ValueError('Build resource leaf outside directory')
    address, length, _, _ = r.unpack('<IIII', position(resource + leaf, 16))
    if not 1 <= length <= 16384:
        raise ValueError('Embedded build identity size is invalid')
    value = json.loads(r.block(position(address, length), length).decode('utf-8'))
    validate_identity(value)
    if value['platform'] != 'Windows' or value['configuration'] != 'Release':
        raise ValueError('Bundle requires a Windows Release runtime')
    if value['source_origin'] not in ('git', 'archive'):
        raise ValueError('Runtime lacks source provenance')
    for field, size in [('source_revision', 40), ('source_tree', 40), ('source_fingerprint', 64)]:
        if not re.fullmatch('[0-9a-f]{' + str(size) + '}', value[field]):
            raise ValueError('Invalid embedded source identity: ' + field)
    return value


def file_record(root, path):
    return {'path': path.relative_to(root).as_posix(), 'bytes': path.stat().st_size,
            'sha256': digest(path)}


def records(root):
    return [file_record(root, path) for path in sorted(root.rglob('*'))
            if path.is_file() and path.relative_to(root).as_posix() != MANIFEST]


def closure(root):
    files = {}
    for path in root.iterdir():
        if path.is_file() and path.suffix.lower() in ('.exe', '.dll'):
            name = path.name.lower()
            if name in files:
                raise ValueError('Windows case-insensitive file collision: ' + name)
            files[name] = path
    if 'judas.exe' not in files:
        raise ValueError('Bundle has no judas.exe')
    external = set()
    visited = set()
    pending = ['judas.exe']
    while pending:
        name = pending.pop()
        if name in visited:
            continue
        visited.add(name)
        info = pe(files[name].read_bytes())
        if info['machine'] != 0x8664 or info['format'] != 'PE32+':
            raise ValueError('Non-x64 runtime dependency: ' + name)
        for dependency in info['needed']:
            if system_dll(dependency):
                if dependency in files:
                    raise ValueError('Refusing bundled Windows system DLL: ' + dependency)
                external.add(dependency)
            elif dependency not in files:
                raise ValueError(name + ': missing runtime DLL ' + dependency)
            else:
                pending.append(dependency)
    if set(files) != visited:
        raise ValueError('Unexpected/unreferenced runtime PE files: ' + ', '.join(sorted(set(files) - visited)))
    return sorted(external)


def verify(root):
    root = Path(root).resolve()
    manifest = json.loads((root / MANIFEST).read_text())
    if manifest.get('schema') != 1 or manifest.get('kind') != 'judas-windows-runtime':
        raise ValueError('Invalid Windows runtime bundle schema')
    if any(path.is_symlink() for path in root.rglob('*')):
        raise ValueError('Runtime bundle symlinks are not allowed')
    if records(root) != manifest['files']:
        raise ValueError('Runtime bundle inventory/checksum mismatch')
    actual = embedded_identity((root / 'judas.exe').read_bytes())
    if actual != manifest['build']:
        raise ValueError('Manifest does not match actual embedded runtime identity')
    if closure(root) != manifest['systemImports']:
        raise ValueError('Runtime dependency closure differs from manifest')
    required = ['SDL2.txt', 'GLM.txt', 'MinGW-w64.txt']
    if any('libstdc++' in row['path'] or 'libgcc_' in row['path'] for row in manifest['files']):
        required.append('GNU-runtime.txt')
    for name in required:
        if not (root / 'licenses' / name).is_file():
            raise ValueError('Required dependency notice missing: ' + name)
    return manifest


def prepare(build, output):
    build, output = Path(build).resolve(), Path(output).absolute()
    identity = embedded_identity((build / 'judas.exe').read_bytes())
    if output.exists() or output.is_symlink():
        raise ValueError('Choose a new runtime bundle directory; existing content is never overwritten')
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.judas-windows-runtime-', dir=output.parent))
    try:
        shutil.copy2(build / 'judas.exe', stage / 'judas.exe')
        sources = {}
        for directory in (build / 'windows-runtime', build):
            for path in directory.glob('*.dll'):
                name = path.name.lower()
                if name in sources and digest(sources[name]) != digest(path):
                    raise ValueError('Conflicting dependency candidates: ' + name)
                sources[name] = path
        pending, copied = ['judas.exe'], set()
        while pending:
            for name in pe((stage / pending.pop()).read_bytes())['needed']:
                if system_dll(name) or name in copied:
                    continue
                if name not in sources:
                    raise ValueError('Cross build did not stage required DLL: ' + name)
                shutil.copy2(sources[name], stage / name)
                copied.add(name)
                pending.append(name)
        shutil.copytree(build / 'windows-licenses', stage / 'licenses')
        manifest = {'schema': 1, 'kind': 'judas-windows-runtime', 'build': identity,
                    'systemImports': closure(stage), 'files': records(stage),
                    'qualification': 'PE metadata and closure only; genuine Windows execution required'}
        (stage / MANIFEST).write_text(json.dumps(manifest, indent=2) + '\n')
        verify(stage)
        stage.rename(output)
        return manifest
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    make = commands.add_parser('prepare')
    make.add_argument('build', type=Path)
    make.add_argument('output', type=Path)
    check = commands.add_parser('verify')
    check.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        result = prepare(args.build, args.output) if args.command == 'prepare' else verify(args.output)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, KeyError, struct.error) as error:
        parser.exit(1, 'Windows runtime bundle failed: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
