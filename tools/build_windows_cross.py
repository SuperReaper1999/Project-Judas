#!/usr/bin/env python3
"""Optional native Windows x64 cross-build/export from Linux; never starts a VM.

Requires CMake, GNU make, Python 3.11+, native build prerequisites and MinGW-w64.
All engine dependencies use the repository's pinned local source archives.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
from windows_runtime import prepare, verify
from export_windows import export


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/windows-cross-Experimental')
    parser.add_argument('--host-build', type=Path, default=ROOT / 'build/linux-cross-host-Experimental')
    parser.add_argument('--mingw-root', type=Path, help='Optional relocated prefix with bin/ and x86_64-w64-mingw32/')
    parser.add_argument('--icu-host-build', type=Path, help='Reuse an existing ICU 76.1 native build/config/icucross.mk')
    parser.add_argument('--channel', choices=['Alpha', 'Beta', 'Experimental', 'Stable', 'Local'], default='Experimental')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--project', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--bundle', type=Path, help='New verified runtime-bundle destination')
    parser.add_argument('--log-dir', type=Path, help='New evidence directory (default inside cross build)')
    args = parser.parse_args()
    if platform.system() != 'Linux':
        parser.error('This optional entry point is for a Linux host; use Build-Windows.ps1 for native MSVC')
    if args.jobs < 1 or args.jobs > 64:
        parser.error('--jobs must be 1–64')
    if bool(args.project) != bool(args.output):
        parser.error('--project and --output must be supplied together')
    prefix = args.mingw_root.resolve() if args.mingw_root else None
    compiler = prefix / 'bin/x86_64-w64-mingw32-g++-posix' if prefix else None
    if not compiler or not compiler.is_file():
        compiler = shutil.which('x86_64-w64-mingw32-g++-posix') or shutil.which('x86_64-w64-mingw32-g++')
    if not compiler:
        parser.error('MinGW-w64 x64 C++ compiler not found. Install g++-mingw-w64-x86-64-posix, or pass --mingw-root PREFIX')
    build, host = args.build.resolve(), args.host_build.resolve()
    if build == host:
        parser.error('Native and cross build directories must be separate')
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    logs = args.log_dir.resolve() if args.log_dir else build / ('cross-evidence-' + stamp)
    logs.mkdir(parents=True, exist_ok=False)
    result = {'schema': 1, 'utc': stamp, 'host': platform.platform(), 'channel': args.channel,
              'crossCompiler': str(compiler), 'runs': [], 'windowsExecution': 'not performed by this build tool'}

    def run(name, command):
        started = time.monotonic()
        with (logs / (name + '.log')).open('w') as stream:
            done = subprocess.run(list(map(str, command)), cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
        result['runs'].append({'name': name, 'argv': list(map(str, command)), 'exit': done.returncode,
                               'seconds': time.monotonic() - started})
        (logs / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
        print(name, done.returncode, flush=True)
        if done.returncode:
            raise RuntimeError(name + ' failed; see ' + str(logs / (name + '.log')))

    try:
        configure = ['cmake', '-S', ROOT, '-B', build,
                     '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/toolchains/WindowsMinGW64.cmake'),
                     '-DCMAKE_BUILD_TYPE=Release', '-DJUDAS_BUILD_CHANNEL=' + args.channel,
                     '-DJUDAS_CROSS_DEPENDENCY_JOBS=' + str(args.jobs)]
        if prefix:
            configure.append('-DJUDAS_MINGW_ROOT=' + str(prefix))
        if args.icu_host_build:
            configure.append('-DJUDAS_ICU_HOST_TOOLS=' + str(args.icu_host_build.resolve()))
        run('cross-configure', configure)
        run('cross-build', ['cmake', '--build', build, '--target', 'judas', 'judas_export',
                            'judas_script_check', '-j', args.jobs])
        destination = args.bundle.resolve() if args.bundle else build / ('runtime-' + stamp)
        manifest = prepare(build, destination)
        result['runtimeBundle'] = str(destination)
        result['runtimeIdentity'] = manifest['build']
        if args.project:
            run('host-configure', ['cmake', '-S', ROOT, '-B', host, '-DCMAKE_BUILD_TYPE=Release',
                                   '-DJUDAS_BUILD_CHANNEL=' + args.channel])
            run('host-build', ['cmake', '--build', host, '--target', 'judas', 'judas_export', '-j', args.jobs])
            package = export(args.project, args.output, destination, host / 'judas_export', host / 'judas', ROOT)
            result['package'] = str(args.output.resolve())
            result['packageFiles'] = len(package['files'])
        result['metadataVerified'] = bool(verify(destination))
        (logs / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        result['error'] = str(error)
        (logs / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
