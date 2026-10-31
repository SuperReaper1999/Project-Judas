#!/usr/bin/env python3
"""Serve only an explicitly selected experimental web package on localhost."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path

class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy','same-origin')
        self.send_header('Cross-Origin-Embedder-Policy','require-corp')
        self.send_header('Cross-Origin-Resource-Policy','same-origin')
        self.send_header('Cache-Control','no-store')
        super().end_headers()

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('package',type=Path);p.add_argument('--port',type=int,default=8080);a=p.parse_args()
    if not (a.package/'BROWSER_BUILD.json').is_file():p.error('Choose a completed browser package, not a source/home directory')
    server=ThreadingHTTPServer(('127.0.0.1',a.port),partial(Handler,directory=str(a.package.resolve())))
    print('Experimental browser game: http://127.0.0.1:'+str(server.server_address[1]),flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()
