#!/usr/bin/env python3
"""Bound repository-local generated storage. Never delete or rotate unknown data."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / 'tools/storage-policy.json'


def usage(path):
    """Count allocated bytes once per inode; reject aliases instead of following them."""
    path = Path(path)
    if path.is_symlink():
        raise ValueError(f'Storage root must not be a symlink: {path}')
    if not path.exists():
        return 0
    seen = set()
    total = 0
    for parent, directories, files in os.walk(path, followlinks=False):
        for name in ['.', *directories, *files]:
            item = Path(parent) if name == '.' else Path(parent) / name
            try:
                info = item.lstat()
            except FileNotFoundError:
                continue  # Concurrent output rename/removal.
            key = (info.st_dev, info.st_ino)
            if key not in seen:
                seen.add(key)
                total += getattr(info, 'st_blocks', 0) * 512 if hasattr(info, 'st_blocks') else info.st_size
    return total


def check(root=ROOT, policy=None):
    root = Path(root).resolve()
    policy = policy if policy is not None else json.loads(POLICY.read_text())
    result = {}
    for name, limit in policy['directory_limits_bytes'].items():
        if name not in ('.cache', 'build') or not isinstance(limit, int) or limit <= 0:
            raise ValueError('Policy may name only positive budgets for .cache and build')
        path = root / name
        if path.is_symlink() or path.resolve() != path or os.path.ismount(path):
            raise ValueError(f'Generated root is aliased or mounted: {path}')
        used = usage(path)
        result[name] = {'allocated_bytes': used, 'limit_bytes': limit}
        if used >= limit:
            raise ValueError(f'{path}: {used} bytes reaches {limit}-byte budget; '
                             'stop and review owned outputs; nothing was deleted')
    if sum(x['allocated_bytes'] for x in result.values()) >= policy['total_limit_bytes']:
        raise ValueError('Combined generated-storage budget reached; nothing was deleted')
    return result


def stop(process):
    if os.name == 'posix':
        # The command has a dedicated session, so never signal the caller's group.
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            return
    else:
        process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        if os.name != 'posix':
            process.kill()
    if os.name == 'posix':
        # A parent can exit before a TERM-ignoring compiler/grandchild does.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    if process.poll() is None:
        process.wait()


def run(command, root=ROOT, interval=5):
    check(root)
    process = subprocess.Popen(command, cwd=root, start_new_session=os.name == 'posix')
    try:
        while process.poll() is None:
            time.sleep(interval)
            check(root)
        check(root)
        return process.returncode
    finally:
        # Also terminate leftover build descendants after the command fails/exits.
        if os.name == 'posix' or process.poll() is None:
            stop(process)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['check', 'run'])
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        if args.action == 'check':
            print(json.dumps(check(), indent=2))
            return 0
        command = args.command[1:] if args.command[:1] == ['--'] else args.command
        if not command:
            parser.error('run requires a command after --')
        return run(command)
    except (OSError, ValueError, KeyboardInterrupt) as error:
        print(f'Storage guard stopped: {error}', flush=True)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
