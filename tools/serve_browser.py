#!/usr/bin/env python3
"""Serve only an explicitly selected experimental web package on localhost."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
import sys
sys.dont_write_bytecode = True
from browser_package import verify_package

class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, allowed=None, **kwargs):
        self.allowed = allowed or set()
        super().__init__(*args, **kwargs)

    def send_head(self):
        from urllib.parse import urlsplit, unquote
        name = unquote(urlsplit(self.path).path).lstrip('/') or 'index.html'
        if name not in self.allowed:
            self.send_error(404, 'File is not part of this package')
            return None
        root = Path(self.directory).resolve()
        path = root / name
        if any(part.is_symlink() for part in [path, *path.parents] if part != root.parent) or not path.resolve().is_relative_to(root):
            self.send_error(403, 'Package symlinks are not served')
            return None
        return super().send_head()

    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy','same-origin')
        self.send_header('Cross-Origin-Embedder-Policy','require-corp')
        self.send_header('Cross-Origin-Resource-Policy','same-origin')
        self.send_header('Cache-Control','no-store')
        super().end_headers()

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('package',type=Path,nargs='?',default=Path(__file__).resolve().parent);p.add_argument('--port',type=int,default=8080);p.add_argument('--verify-only',action='store_true');a=p.parse_args()
    if not 0 <= a.port <= 65535:p.error('Port must be between 0 and 65535 (0 chooses a free port)')
    try:receipt=verify_package(a.package)
    except (OSError,ValueError) as error:p.error(str(error))
    if a.verify_only:
        print('Verified browser package: '+str(len(receipt['files']))+' files; all SHA-256 hashes match');p.exit()
    try:server=ThreadingHTTPServer(('127.0.0.1',a.port),partial(Handler,directory=str(a.package.resolve()),allowed=set(receipt['files'])|{'BROWSER_BUILD.json'}))
    except OSError as error:p.error('Cannot start localhost preview; choose another port: '+str(error))
    print('Experimental browser game: http://127.0.0.1:'+str(server.server_address[1]),flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()
