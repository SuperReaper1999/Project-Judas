"""Real HTTP, deterministic compression and malformed-sidecar boundaries."""
import contextlib
import gzip
import hashlib
import http.client
import importlib.util
import json
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]

def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT/'tools'/(name+'.py'))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

package = load('browser_package')
with patch.dict(sys.modules, {'browser_package': package}):
    preview = load('serve_browser')


def write_receipt(root, encodings):
    files = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
             for p in root.iterdir() if p.is_file() and p.name != 'BROWSER_BUILD.json'}
    data = dict(schema=1, kind='judas-browser-experimental', experimentalOptIn=True,
                files=files, encodings=encodings)
    (root/'BROWSER_BUILD.json').write_text(json.dumps(data), encoding='utf-8')


class DeliveryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ['index.html', 'judas_browser.js', 'judas_browser.wasm',
                     'game.data', 'game.data.js', 'game-config.js']:
            (self.root/name).write_bytes((b'fixture '+name.encode()+b'\n')*1000)
        self.encodings = package.compress_payloads(self.root)
        write_receipt(self.root, self.encodings)

    @contextlib.contextmanager
    def server(self):
        receipt = package.verify_package(self.root)
        stamps = {name: preview.signature((self.root/name).stat())
                  for name in [*receipt['files'], 'BROWSER_BUILD.json']}
        from functools import partial
        handler = partial(preview.Handler, directory=str(self.root), receipt=receipt, stamps=stamps)
        server = preview.ThreadingHTTPServer(('127.0.0.1', 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            yield server.server_address[1]
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)

    def request(self, port, name='judas_browser.wasm', encoding='gzip', etag=None, method='GET'):
        headers = {'Accept-Encoding': encoding}
        if etag is not None:
            headers['If-None-Match'] = etag
        conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
        try:
            conn.request(method, '/'+name, headers=headers)
            response = conn.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            conn.close()

    def test_deterministic_sidecars_retain_originals(self):
        original = {name: (self.root/name).read_bytes() for name in self.encodings}
        first = {name: (self.root/variant['gzip']).read_bytes() for name, variant in self.encodings.items()}
        self.assertEqual(package.compress_payloads(self.root), self.encodings)
        for name, data in first.items():
            self.assertEqual(data, (self.root/(name+'.gz')).read_bytes())
            self.assertEqual(data[4:8], b'\0'*4, 'gzip timestamp must not depend on build time')
            self.assertEqual(gzip.decompress(data), original[name])
            self.assertEqual((self.root/name).read_bytes(), original[name])

    def test_small_and_incompressible_files_keep_identity_only(self):
        (self.root/'small.js').write_bytes(b'x'*100)
        (self.root/'random.data').write_bytes(random.Random(42).randbytes(16384))
        variants = package.compress_payloads(self.root)
        for name in ['small.js', 'random.data']:
            self.assertNotIn(name, variants)
            self.assertFalse((self.root/(name+'.gz')).exists())

    def test_sidecar_content_must_match_not_just_its_own_hash(self):
        name='judas_browser.wasm'
        (self.root/(name+'.gz')).write_bytes(gzip.compress(b'x'*(self.root/name).stat().st_size))
        write_receipt(self.root, self.encodings)
        with self.assertRaisesRegex(ValueError, 'differs from original'):
            package.verify_package(self.root)

    def test_decompression_is_bounded_by_verified_original_size(self):
        (self.root/'judas_browser.wasm.gz').write_bytes(gzip.compress(b'x'*(2*1024*1024)))
        write_receipt(self.root, self.encodings)
        with self.assertRaisesRegex(ValueError, 'exceeds original'):
            package.verify_package(self.root)

    def test_truncated_gzip_rejects_even_with_matching_inventory_hash(self):
        p=self.root/'judas_browser.wasm.gz';p.write_bytes(p.read_bytes()[:-4])
        write_receipt(self.root, self.encodings)
        with self.assertRaisesRegex(ValueError, 'Invalid gzip representation'):
            package.verify_package(self.root)

    def test_malformed_sidecar_mapping_rejects(self):
        for mapping in [[], {'absent': {'gzip': 'absent.gz'}},
                        {'game.data': {'gzip': '../game.data.gz'}},
                        {'game.data': {'br': 'game.data.gz'}},
                        {'game.data': {'gzip': ['game.data.gz']}}]:
            write_receipt(self.root, mapping)
            with self.assertRaises(ValueError):
                package.verify_package(self.root)

    def test_real_http_negotiation_mime_and_identity_fallback(self):
        with self.server() as port:
            for encoding, compressed in [('gzip, deflate, br', True), ('gzip;q=0', False),
                                          ('br', False), ('*', True),
                                          ('gzip;q=0.5, identity;q=1', False),
                                          ('identity;q=0, gzip;q=1', True)]:
                status, headers, body = self.request(port, encoding=encoding)
                self.assertEqual(status, 200, encoding)
                self.assertEqual(headers['Content-Type'], 'application/wasm')
                self.assertEqual(headers['Vary'], 'Accept-Encoding')
                self.assertEqual(int(headers['Content-Length']), len(body))
                self.assertEqual(headers.get('Content-Encoding'), 'gzip' if compressed else None)
                self.assertEqual(gzip.decompress(body) if compressed else body,
                                 (self.root/'judas_browser.wasm').read_bytes())
                self.assertEqual(headers['Cross-Origin-Embedder-Policy'], 'require-corp')
            for encoding in ['identity;q=0, gzip;q=0', '*;q=0']:
                status, headers, _ = self.request(port, encoding=encoding)
                self.assertEqual(status, 406)
                self.assertEqual(headers['Cache-Control'], 'no-store')

    def test_variant_specific_etags_and_conditional_requests(self):
        with self.server() as port:
            _, identity, _ = self.request(port, encoding='identity')
            _, zipped, _ = self.request(port)
            self.assertNotEqual(identity['ETag'], zipped['ETag'])
            for encoding, headers in [('identity', identity), ('gzip', zipped)]:
                for tag in [headers['ETag'], 'W/'+headers['ETag'], '*']:
                    status, h, body = self.request(port, encoding=encoding, etag=tag)
                    self.assertEqual(status, 304)
                    self.assertEqual(body, b'')
                    self.assertEqual(h['Cache-Control'], 'no-cache')
                    self.assertEqual(h['Vary'], 'Accept-Encoding')
                other = zipped if encoding=='identity' else identity
                self.assertEqual(self.request(port, encoding=encoding, etag=other['ETag'])[0], 200)

    def test_http11_framing_keeps_connection_usable_after_304_and_error(self):
        with self.server() as port:
            conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
            try:
                conn.request('GET', '/game.data', headers={'Accept-Encoding': 'gzip'})
                response=conn.getresponse()
                self.assertEqual(response.version, 11)
                tag=response.getheader('ETag');self.assertTrue(response.read())
                conn.request('GET', '/game.data', headers={'Accept-Encoding': 'gzip', 'If-None-Match': tag})
                response=conn.getresponse();self.assertEqual(response.status, 304)
                self.assertEqual(response.read(), b'')
                conn.request('HEAD', '/game.data', headers={'Accept-Encoding': 'gzip'})
                response=conn.getresponse();self.assertEqual(response.status, 200)
                self.assertEqual(response.read(), b'')
                conn.request('GET', '/missing')
                response=conn.getresponse();self.assertEqual(response.status, 404)
                self.assertTrue(response.read())
                conn.request('GET', '/game.data')
                response=conn.getresponse();self.assertEqual(response.status, 200)
                self.assertTrue(response.read())
            finally:
                conn.close()

    def test_head_has_selected_length_without_body(self):
        with self.server() as port:
            status, headers, body = self.request(port, method='HEAD')
            self.assertEqual(status, 200)
            self.assertEqual(body, b'')
            self.assertEqual(int(headers['Content-Length']), (self.root/'judas_browser.wasm.gz').stat().st_size)

    def test_modified_payload_or_receipt_fails_closed_even_on_cache_hit(self):
        for name in ['judas_browser.wasm', 'judas_browser.wasm.gz', 'BROWSER_BUILD.json']:
            with self.subTest(name=name):
                original=(self.root/name).read_bytes()
                with self.server() as port:
                    _, headers, _ = self.request(port)
                    (self.root/name).write_bytes(original+b'changed')
                    status, h, _ = self.request(port, etag=headers['ETag'])
                    self.assertEqual(status, 503)
                    self.assertEqual(h['Cache-Control'], 'no-store')
                (self.root/name).write_bytes(original)


class LoadingTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('node'), 'Node is required for shell loading checks')
    def test_actual_shell_status_and_failure_paths(self):
        result = subprocess.run([shutil.which('node'), str(ROOT/'tests/browser/loading.cjs')],
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertIn('8 shell loading checks passed', result.stdout)


if __name__ == '__main__':
    unittest.main()
