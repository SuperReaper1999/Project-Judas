"""Default-off browser tooling boundaries; no SDK/browser download required."""
import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_browser', ROOT/'tools/build_browser.py')
browser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(browser)

class BrowserExportTests(unittest.TestCase):
    def parse(self, *options):
        return browser.parse_args(['--project', '/tmp/game.judasproj', '--output', '/tmp/web', *options])

    def denied(self, *options):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.parse(*options)

    def test_default_and_explicit_zero_are_disabled(self):
        self.denied()
        self.denied('-ExpBrowserSupport=0')

    def test_exact_opt_in_and_documented_spacing(self):
        for args in [('-ExpBrowserSupport=1',), ('-ExpBrowserSupport','1'), ('-ExpBrowserSupport','=','1')]:
            self.assertEqual(self.parse(*args).ExpBrowserSupport, '1')

    def test_malformed_abbreviated_and_duplicate_switches_reject(self):
        for args in [('-ExpBrowserSupport=true',), ('-ExpBrowser=1',), ('-ExpBrowserSupport',),
                     ('-ExpBrowserSupport=0','-ExpBrowserSupport=1'),
                     ('-ExpBrowserSupport=1','-ExpBrowserSupport=1')]:
            self.denied(*args)

    def test_native_platforms_need_no_emscripten(self):
        with patch.object(browser.sys, 'platform', 'win32'):
            self.denied('-ExpBrowserSupport=1')

    def test_actual_legacy_project_and_policy_controls(self):
        source=(ROOT/'projects/2d_traversal_workshop/2D_Traversal_Workshop.judasproj').read_text(encoding='utf-8')
        browser.require_supported_project(source)
        for text in [source.replace('advanced-fluid-dynamics "false"','advanced-fluid-dynamics "true"'),
                     source.replace('legacy-gameplay "false"','legacy-gameplay "true"')]:
            with self.assertRaisesRegex(ValueError, 'script-owned gameplay'):
                browser.require_supported_project(text)

    def test_named_format_cannot_bypass_policy(self):
        browser.require_supported_project('{"data":{"legacyGameplay":false,"advancedFluidDynamics":false}}')
        for text in ['{"data":{"legacyGameplay":false,"advancedFluidDynamics":true}}',
                     '{"data":{"legacyGameplay":true}}', '{}']:
            with self.assertRaises(ValueError):
                browser.require_supported_project(text)

    def test_web_runtime_notices_are_included_verbatim(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);sdk=root/'sdk';output=root/'web';output.mkdir()
            for path in ['LICENSE','system/lib/libc/musl/COPYRIGHT','system/lib/compiler-rt/LICENSE.TXT',
                         'system/lib/libcxx/LICENSE.TXT','system/lib/libcxxabi/LICENSE.TXT','system/lib/libunwind/LICENSE.TXT']:
                source=sdk/path;source.parent.mkdir(parents=True,exist_ok=True);source.write_text('Notice '+path)
            browser.copy_browser_licenses(sdk,output)
            self.assertEqual((output/'licenses/emscripten-LICENSE.txt').read_text(),'Notice LICENSE')
            self.assertEqual((output/'licenses/SDL-LICENSE.txt').read_bytes(),(ROOT/'third_party/android/SDL-LICENSE.txt').read_bytes())
            self.assertEqual((output/'licenses/Judas-LICENSE.txt').read_bytes(),(ROOT/'LICENSE').read_bytes())

    def test_missing_web_runtime_notice_rejects_packaging(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError,'redistribution notice missing'):
                browser.copy_browser_licenses(Path(tmp)/'absent-sdk',Path(tmp))

    def test_existing_output_is_preserved_before_any_process_runs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); project=root/'game.judasproj';project.write_text('unchanged')
            output=root/'web';output.mkdir();(output/'keep').write_text('keep')
            with patch.object(browser.subprocess, 'check_output') as probe:
                with self.assertRaisesRegex(ValueError, 'fresh directory'):
                    browser.main(['-ExpBrowserSupport=1','--project',str(project),'--output',str(output)])
                probe.assert_not_called()
            self.assertEqual((output/'keep').read_text(), 'keep')

    def test_output_inside_project_rejects_without_creating_it(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);project=root/'game.judasproj';project.write_text('unchanged');output=root/'web'
            with self.assertRaisesRegex(ValueError, 'outside the project'):
                browser.main(['-ExpBrowserSupport=1','--project',str(project),'--output',str(output)])
            self.assertFalse(output.exists())

if __name__ == '__main__':
    unittest.main()
