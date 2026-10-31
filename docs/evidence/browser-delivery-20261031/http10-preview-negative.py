#!/usr/bin/env python3
"""Serve an immutable, verified experimental web package on localhost."""
import argparse
import hashlib
import os
import re
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit
import sys
sys.dont_write_bytecode = True
from browser_package import verify_package


def encoding_quality(header, encoding):
    values = {}
    for item in (header or '').split(','):
        parts = [p.strip().lower() for p in item.split(';')]
        if not parts[0]:
            continue
        quality = 1.0
        if len(parts) > 1:
            match = re.fullmatch(r'q\s*=\s*(0(?:\.\d{0,3})?|1(?:\.0{0,3})?)', parts[1])
            quality = float(match[1]) if match and len(parts) == 2 else 0.0
        values[parts[0]] = min(values.get(parts[0], 1.0), quality)
    if encoding in values:
        return values[encoding]
    if encoding == 'identity':
        return 0.0 if values.get('*') == 0 else 1.0
    return values.get('*', 0.0)


def signature(stat):
    return stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, receipt=None, stamps=None, **kwargs):
        self.receipt = receipt or {}
        self.stamps = stamps or {}
        self._cacheable = False
        self._vary = False
        super().__init__(*args, **kwargs)

    def safe_path(self, name):
        root = Path(self.directory).resolve()
        path = root / name
        if any(p.is_symlink() for p in [path, *path.parents] if p != root.parent) or not path.resolve().is_relative_to(root):
            raise ValueError('Package symlinks are not served')
        if not path.is_file() or signature(path.stat()) != self.stamps[name]:
            raise ValueError('Package changed after verification; restart the preview')
        return path

    def send_head(self):
        self._cacheable = self._vary = False
        name = unquote(urlsplit(self.path).path).lstrip('/') or 'index.html'
        files = self.receipt.get('files', {})
        if name not in files and name != 'BROWSER_BUILD.json':
            self.send_error(404, 'File is not part of this package')
            return None
        try:
            self.safe_path('BROWSER_BUILD.json')
            original = self.safe_path(name)
            variants = self.receipt.get('encodings', {}).get(name, {})
            self._vary = bool(variants)
            gzip_quality = encoding_quality(self.headers.get('Accept-Encoding'), 'gzip')
            identity_quality = encoding_quality(self.headers.get('Accept-Encoding'), 'identity')
            encoded = bool(variants) and gzip_quality > 0 and gzip_quality >= identity_quality
            if not encoded and identity_quality <= 0:
                self.send_error(406, 'No acceptable content encoding')
                return None
            selected = variants['gzip'] if encoded else name
            path = self.safe_path(selected)
            digest = files[selected] if selected in files else hashlib.sha256(path.read_bytes()).hexdigest()
            etag = '"' + digest + '"'
            tags = [tag.strip().removeprefix('W/') for tag in self.headers.get('If-None-Match', '').split(',')]
            self._cacheable = True
            if etag in tags or '*' in tags:
                self.send_response(304)
                self.send_header('ETag', etag)
                self.end_headers()
                return None
            stream = path.open('rb')
            if signature(os.fstat(stream.fileno())) != self.stamps[selected]:
                stream.close()
                raise ValueError('Package changed after verification; restart the preview')
        except (OSError, ValueError, KeyError) as error:
            self._cacheable = False
            self.send_error(503, str(error))
            return None
        self.send_response(200)
        media_type = {'.wasm': 'application/wasm', '.js': 'application/javascript', '.data': 'application/octet-stream'}.get(original.suffix, self.guess_type(str(original)))
        self.send_header('Content-Type', media_type)
        self.send_header('Content-Length', str(path.stat().st_size))
        self.send_header('ETag', etag)
        if encoded:
            self.send_header('Content-Encoding', 'gzip')
        self.end_headers()
        return stream

    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Resource-Policy', 'same-origin')
        self.send_header('Cache-Control', 'no-cache' if self._cacheable else 'no-store')
        if self._vary:
            self.send_header('Vary', 'Accept-Encoding')
        super().end_headers()


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('package', type=Path, nargs='?', default=Path(__file__).resolve().parent)
    p.add_argument('--port', type=int, default=8080)
    p.add_argument('--verify-only', action='store_true')
    a = p.parse_args()
    if not 0 <= a.port <= 65535:
        p.error('Port must be between 0 and 65535 (0 chooses a free port)')
    try:
        receipt = verify_package(a.package)
        stamps = {name: signature((a.package/name).stat()) for name in [*receipt['files'], 'BROWSER_BUILD.json']}
    except (OSError, ValueError) as error:
        p.error(str(error))
    if a.verify_only:
        print('Verified browser package: '+str(len(receipt['files']))+' files; all SHA-256 hashes match')
        p.exit()
    try:
        server = ThreadingHTTPServer(('127.0.0.1', a.port), partial(Handler, directory=str(a.package.resolve()), receipt=receipt, stamps=stamps))
    except OSError as error:
        p.error('Cannot start localhost preview; choose another port: '+str(error))
    print('Experimental browser game: http://127.0.0.1:'+str(server.server_address[1]), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
