"""Read-only browser preflight and real local-hosting/relocation boundaries."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import queue
import threading
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from urllib.request import urlopen
from urllib.error import HTTPError

ROOT = Path(__file__).resolve().parents[1]

def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT/'tools'/(name+'.py'))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

compiler, package = module('build_browser'), module('browser_package')

def receipt(root):
    files = {p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
             for p in root.rglob('*') if p.is_file() and p.name != 'BROWSER_BUILD.json'}
    (root/'BROWSER_BUILD.json').write_text(json.dumps({'schema':1,'kind':'judas-browser-experimental',
        'experimentalOptIn':True,'files':files}), encoding='utf-8')

def fixture(root):
    root.mkdir()
    for name in ['index.html','judas_browser.js','judas_browser.wasm','game.data','game.data.js','game-config.js']:
        (root/name).write_bytes(('fixture '+name).encode())
    shutil.copyfile(ROOT/'tools/serve_browser.py',root/'serve.py')
    shutil.copyfile(ROOT/'tools/browser_package.py',root/'browser_package.py')
    receipt(root)

class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)/'package';fixture(self.root)

    def test_complete_inventory(self):
        self.assertEqual(len(package.verify_package(self.root)['files']),8)

    def test_hash_mismatch(self):
        (self.root/'game.data').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'integrity mismatch'):package.verify_package(self.root)

    def test_missing_payload(self):
        (self.root/'game.data').unlink()
        with self.assertRaisesRegex(ValueError,'Missing package files'):package.verify_package(self.root)

    def test_unlisted_secret_is_rejected(self):
        (self.root/'private.txt').write_text('not exported')
        with self.assertRaisesRegex(ValueError,'Unlisted file'):package.verify_package(self.root)

    def test_parent_and_absolute_inventory_paths(self):
        for name in ['../outside','/etc/passwd','a/../index.html','a\\b','./index.html']:
            data=json.loads((self.root/'BROWSER_BUILD.json').read_text());data['files'][name]='0'*64
            (self.root/'BROWSER_BUILD.json').write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError,'Unsafe'):package.verify_package(self.root)
            receipt(self.root)

    @unittest.skipIf(os.name=='nt','symlink privileges vary on Windows')
    def test_symlink_is_rejected_even_when_hash_matches(self):
        target=self.root.parent/'outside';target.write_text('fixture game.data')
        (self.root/'game.data').unlink();(self.root/'game.data').symlink_to(target)
        with self.assertRaisesRegex(ValueError,'Symlinks'):package.verify_package(self.root)

    def test_malformed_and_oversized_receipt(self):
        for value in ['null','[]','{}','not JSON',' '* (2*1024*1024+1)]:
            (self.root/'BROWSER_BUILD.json').write_text(value)
            with self.assertRaises(ValueError):package.verify_package(self.root)

    def test_relocated_helper_and_real_headers(self):
        moved=self.root.parent/'moved package';shutil.move(self.root,moved)
        result=subprocess.run([sys.executable,str(moved/'serve.py'),'--verify-only'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertFalse((moved/'__pycache__').exists(),'preview must not mutate its own inventory')
        server=subprocess.Popen([sys.executable,str(moved/'serve.py'),'--port','0'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            ready=queue.Queue()
            threading.Thread(target=lambda:ready.put(server.stdout.readline()),daemon=True).start()
            try:line=ready.get(timeout=5)
            except queue.Empty:self.fail('preview did not report readiness')
            self.assertIn('http://127.0.0.1:',line)
            url=line.split()[-1]
            with urlopen(url,timeout=3) as response:
                self.assertEqual(response.read(),b'fixture index.html')
                self.assertEqual(response.headers['Cross-Origin-Opener-Policy'],'same-origin')
                self.assertEqual(response.headers['Cross-Origin-Embedder-Policy'],'require-corp')
            (moved/'late-secret').write_text('not in receipt')
            for name in ['late-secret','../outside','%2e%2e/outside']:
                with self.assertRaises(HTTPError) as error:urlopen(url+'/'+name,timeout=3)
                error.exception.close()
            (moved/'late-secret').unlink()
        finally:
            server.terminate();server.communicate(timeout=5)
        self.assertFalse((moved/'__pycache__').exists())
        self.assertTrue(package.verify_package(moved))

class PreflightTests(unittest.TestCase):
    def setUp(self):
        platform=patch.object(compiler.sys,'platform','linux');platform.start();self.addCleanup(platform.stop)
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.project=self.root/'project/game.judasproj';self.project.parent.mkdir()
        self.project.write_text('legacy-gameplay "false"\nadvanced-fluid-dynamics "false"\n')
        self.args=compiler.parse_args(['-ExpBrowserSupport=1','--project',str(self.project),'--output',str(self.root/'web'),
           '--build',str(self.root/'cache'),'--emsdk',str(self.root/'sdk'),'--exporter',str(self.root/'exporter'),'--desktop-runtime',str(self.root/'runtime'),'--check'])

    def test_collects_failures_without_creating_any_paths(self):
        with patch.object(compiler.shutil,'which',return_value=None):report=compiler.preflight(self.args)
        self.assertFalse(report['ready']);self.assertGreaterEqual(sum(not c['ok'] for c in report['checks']),6)
        for name in ['web','cache','sdk','exporter','runtime']:self.assertFalse((self.root/name).exists())

    def tools(self):
        sdk=self.root/'sdk';emscripten=sdk/'upstream/emscripten';emscripten.mkdir(parents=True)
        for name in ['emcmake','emcc']:(emscripten/name).touch()
        (emscripten/'emscripten-version.txt').write_text('"6.0.13"');(sdk/'.emscripten').touch()
        for name in ['exporter','runtime']:(self.root/name).touch()
        return {'configuration':'Release','version':'test','source_revision':'a','source_fingerprint':'b','source_untracked_fingerprint':'c','channel':'Experimental'}

    def test_matching_tools_and_opt_in_are_checked_without_build(self):
        identity=self.tools()
        with patch.object(compiler.shutil,'which',return_value='/tool'),patch.object(compiler,'probe',return_value=identity):
            report=compiler.preflight(self.args)
        self.assertTrue(report['ready'],report)
        self.assertFalse((self.root/'web').exists());self.assertFalse((self.root/'cache').exists())

    def test_mismatched_native_tools_fail(self):
        identity=self.tools()
        with patch.object(compiler.shutil,'which',return_value='/tool'),patch.object(compiler,'probe',side_effect=[identity,dict(identity,source_fingerprint='different')]):
            self.assertFalse(compiler.preflight(self.args)['ready'])

    def test_wrong_sdk_and_unsupported_project(self):
        self.tools();(self.root/'sdk/upstream/emscripten/emscripten-version.txt').write_text('"5.0.0"')
        self.project.write_text('legacy-gameplay "true"\n')
        with patch.object(compiler.shutil,'which',return_value='/tool'),patch.object(compiler,'probe',return_value={'version':'invalid'}):
            report=compiler.preflight(self.args)
        self.assertFalse(report['ready']);self.assertFalse(next(c for c in report['checks'] if c['name']=='Saved project')['ok'])
        self.assertFalse(next(c for c in report['checks'] if c['name']=='Emscripten SDK')['ok'])

    def test_shared_output_cache_rejected(self):
        self.args.build=self.args.output
        self.assertFalse(next(c for c in compiler.preflight(self.args)['checks'] if c['name']=='Output folder')['ok'])

if __name__=='__main__':unittest.main()
