"""Strict integrity/inventory check shared by export and the localhost preview."""
import hashlib
import gzip
import json
from pathlib import Path, PurePosixPath


def compress_payloads(root):
    """Deterministic optional transfer representations; retain original files."""
    encodings = {}
    for path in sorted(Path(root).iterdir()):
        if not path.is_file() or path.suffix not in ('.wasm', '.js', '.data') or path.stat().st_size < 4096:
            continue
        target = path.with_name(path.name + '.gz')
        with path.open('rb') as source, target.open('wb') as output:
            with gzip.GzipFile(filename='', fileobj=output, mode='wb', compresslevel=9, mtime=0) as encoded:
                for block in iter(lambda: source.read(1024 * 1024), b''):
                    encoded.write(block)
        if target.stat().st_size < path.stat().st_size * .95:
            encodings[path.name] = {'gzip': target.name}
        else:
            target.unlink()
    return encodings


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
    encodings = data.get('encodings', {})
    if not isinstance(encodings, dict) or len(encodings) > len(files):
        raise ValueError('Invalid encoded representation inventory')
    for original, variants in encodings.items():
        if original not in files or not isinstance(variants, dict) or set(variants) != {'gzip'}:
            raise ValueError('Invalid encoded representation: '+str(original))
        encoded = variants['gzip']
        if encoded != original + '.gz' or encoded not in files:
            raise ValueError('Invalid gzip representation: '+str(original))
        # Hash validation alone cannot establish that a sidecar decodes to the
        # named original. Bound decompression by that verified original's size.
        expected = (root/original).stat().st_size
        digest = hashlib.sha256()
        count = 0
        try:
            with gzip.open(root/encoded, 'rb') as stream:
                while block := stream.read(min(1024*1024, expected-count+1)):
                    count += len(block)
                    if count > expected:
                        raise ValueError('Gzip representation exceeds original: '+original)
                    digest.update(block)
        except (OSError, EOFError) as error:
            raise ValueError('Invalid gzip representation: '+original) from error
        if count != expected or digest.hexdigest() != files[original]:
            raise ValueError('Gzip representation differs from original: '+original)
    return data
