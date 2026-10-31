"""Strict integrity/inventory check shared by export and the localhost preview."""
import hashlib
import json
from pathlib import Path, PurePosixPath


def verify_package(package):
    root = Path(package)
    if root.is_symlink() or not root.is_dir():
        raise ValueError('Choose a real browser package directory, not a symlink')
    root = root.resolve()
    receipt = root / 'BROWSER_BUILD.json'
    if receipt.is_symlink() or not receipt.is_file() or receipt.stat().st_size > 2*1024*1024:
        raise ValueError('Missing or oversized BROWSER_BUILD.json; choose a completed export')
    data = json.loads(receipt.read_text(encoding='utf-8'))
    if not isinstance(data, dict) or data.get('schema') != 1 or data.get('kind') != 'judas-browser-experimental' or data.get('experimentalOptIn') is not True:
        raise ValueError('Unsupported browser package receipt')
    files = data.get('files')
    if not isinstance(files, dict) or not 1 <= len(files) <= 10000:
        raise ValueError('Browser receipt has no bounded file inventory')
    required = {'index.html', 'judas_browser.js', 'judas_browser.wasm', 'game.data', 'game.data.js', 'game-config.js'}
    if not required.issubset(files):
        raise ValueError('Incomplete browser runtime/content inventory')
    for name, digest in files.items():
        if not isinstance(name, str):
            raise ValueError('Inventory paths must be strings')
        p = PurePosixPath(name)
        if not name or '\\' in name or p.is_absolute() or any(part in ('.', '..') for part in name.split('/')) or str(p) != name or name == 'BROWSER_BUILD.json':
            raise ValueError('Unsafe package inventory path: '+name)
        if not isinstance(digest, str) or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
            raise ValueError('Invalid SHA-256 for '+name)
    actual = set()
    for path in root.rglob('*'):
        if path.is_symlink():
            raise ValueError('Symlinks are not allowed in served packages: '+path.relative_to(root).as_posix())
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError('Non-regular package entry')
        name = path.relative_to(root).as_posix()
        if name == 'BROWSER_BUILD.json':
            continue
        if name not in files:
            raise ValueError('Unlisted file in browser package: '+name)
        actual.add(name)
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024*1024), b''):
                digest.update(chunk)
        if digest.hexdigest() != files[name]:
            raise ValueError('Package integrity mismatch: '+name)
    if actual != set(files):
        raise ValueError('Missing package files: '+', '.join(sorted(set(files)-actual)))
    return data
