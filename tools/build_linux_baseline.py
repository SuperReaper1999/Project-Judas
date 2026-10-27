#!/usr/bin/env python3
"""Build a Linux SDK against Ubuntu 24.04, without replacing host libraries.

Uses an explicitly prepared rootfs with bubblewrap, or Docker/Podman with the
repository's build image. Optional tooling; ordinary native builds are unchanged.
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


def environment_command(source, build, *, rootfs=None, engine=None, image=None):
    source, build = Path(source).resolve(), Path(build).resolve()
    if source == build or source.is_relative_to(build):
        raise ValueError('Build directory must not contain or equal the source directory')
    if rootfs:
        rootfs = Path(rootfs).resolve()
        if rootfs == source or rootfs == build or source.is_relative_to(rootfs) or build.is_relative_to(rootfs) or rootfs.is_relative_to(build):
            raise ValueError('Rootfs must be separate from source and build mount destinations')
        # Existing mount points are required because the prepared rootfs is read-only.
        for name in ('source', 'build', 'bin/sh', 'etc/os-release'):
            if not (rootfs / name).exists():
                raise ValueError(f'Prepared rootfs lacks /{name}; see docs/LINUX_BASELINE_BUILD.md')
        return ['bwrap', '--clearenv', '--ro-bind', str(rootfs), '/', '--dev', '/dev', '--proc', '/proc',
                '--tmpfs', '/tmp', '--ro-bind', str(source), '/source',
                '--bind', str(build), '/build', '--unshare-user', '--uid', '0', '--gid', '0',
                '--setenv', 'HOME', '/tmp', '--setenv', 'PATH',
                '/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin',
                '--setenv', 'LANG', 'C.UTF-8', '--chdir', '/source']
    if any(',' in str(path) for path in (source, build)):
        raise ValueError('Container mount paths cannot contain commas; use --rootfs or another directory')
    if engine not in ('docker', 'podman'):
        raise ValueError('Choose Docker/Podman or an explicit --rootfs; no host libraries are replaced')
    # Do not silently pull a newer image or expose the host's home/credentials.
    # A read-only source mount prevents container-side source changes.
    return [engine, 'run', '--rm', '--pull=never', '--network=none',
            '--user', f'{os.getuid()}:{os.getgid()}', '--env', 'HOME=/tmp',
            '--mount', f'type=bind,src={source},dst=/source,readonly',
            '--mount', f'type=bind,src={build},dst=/build', '--workdir', '/source', image]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/linux-ubuntu24-Experimental')
    backend = parser.add_mutually_exclusive_group()
    backend.add_argument('--rootfs', type=Path, help='Prepared Ubuntu 24.04 root for rootless bubblewrap')
    backend.add_argument('--engine', choices=['docker', 'podman'], help='Default: first installed Podman/Docker')
    parser.add_argument('--image', default='judas-build-ubuntu24', help='Explicitly prepared local image; never pulled by this command')
    parser.add_argument('--channel', choices=['Alpha','Beta','Experimental','Stable'], default='Experimental')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--extra-target', action='append', default=[], help='Additional focused native target')
    parser.add_argument('--verify-only', action='store_true', help='Recheck a previously built SDK inside the baseline environment')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'amd64'):
        parser.error('This qualified baseline workflow currently supports Linux x86-64 hosts only')
    if not 1 <= args.jobs <= 32:
        parser.error('--jobs must be 1–32')
    engine = args.engine or next((tool for tool in ('podman', 'docker') if shutil.which(tool)), None)
    build = args.build.resolve()
    try:
        prefix = environment_command(ROOT, build, rootfs=args.rootfs, engine=engine, image=args.image)
        if not shutil.which(prefix[0]):
            raise ValueError(f'{prefix[0]} is unavailable; see docs/LINUX_BASELINE_BUILD.md')
        build.mkdir(parents=True, exist_ok=True)
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        logs = build / 'baseline-evidence' / stamp
        logs.mkdir(parents=True)
        result = {'schema': 1, 'utc': stamp, 'source': str(ROOT), 'build': str(build),
                  'environment': prefix, 'runs': [], 'passed': False}

        def run(name, argv):
            start = time.monotonic()
            command = prefix + argv
            with (logs / (name + '.log')).open('w') as stream:
                completed = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
            result['runs'].append({'name': name, 'argv': command, 'exit': completed.returncode,
                                   'seconds': time.monotonic() - start})
            (logs / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
            print(f'{name}: exit {completed.returncode}; {logs / (name + ".log")}', flush=True)
            if completed.returncode:
                raise RuntimeError(name + ' failed; no compatibility pass is claimed')

        run('baseline-host', ['/bin/sh', '-ec',
            '. /etc/os-release; test "$ID:$VERSION_ID" = ubuntu:24.04; '
            'cat /etc/os-release; ldd --version; c++ --version; cmake --version; '
            'python3 --version; dpkg-query -W'])
        if not args.verify_only:
            run('configure', ['cmake', '-S', '/source', '-B', '/build', '-G', 'Ninja',
                             '-DCMAKE_BUILD_TYPE=Release', '-DJUDAS_BUILD_CHANNEL=' + args.channel])
            run('build', ['cmake', '--build', '/build', '--parallel', str(args.jobs),
                          '--target', 'judas_linux_sdk', *args.extra_target])
        relative = logs.relative_to(build).as_posix()
        run('qualify', ['python3', '/build/linux-sdk/tools/inspect_sdk.py', '/build/linux-sdk',
                        '--host-check', '--max-glibc', '2.39', '--require-os', 'ubuntu:24.04',
                        '--output', '/build/' + relative + '/sdk-qualification.json'])
        result['passed'] = True
        result['limitation'] = 'SDK tool/ABI qualification only; not application, driver or hardware certification'
        (logs / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
        print(f'Qualified SDK: {build / "linux-sdk"}')
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
