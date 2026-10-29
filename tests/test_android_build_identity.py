"""No Android SDK, device, network, native compilation or signing required."""
import copy
import contextlib
import io
import tempfile
from unittest.mock import patch
import sys
import unittest
from pathlib import Path
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import build_android
from build_android import identity_differences, matching_identity, identity_mismatch_message, parse_args

class AndroidIdentityTests(unittest.TestCase):
    def setUp(self):
        self.alpha = dict(version='1.0.2-alpha.1', channel='Alpha', source_revision='abc',
                          source_tree='tree', source_dirty=False, source_fingerprint='fingerprint',
                          source_origin='git', source_untracked_count=0, source_untracked_fingerprint='empty')

    def test_unchanged_candidate(self):
        self.assertTrue(matching_identity(self.alpha, copy.deepcopy(self.alpha)))

    def test_channel_only_advice(self):
        native = dict(self.alpha, channel='Experimental')
        self.assertEqual(identity_differences(self.alpha, native), ['channel'])
        message = identity_mismatch_message('exporter', self.alpha, native)
        self.assertIn("desktop='Alpha', Android='Experimental'", message)
        self.assertIn('--channel Alpha', message)
        self.assertNotIn('rebuild mismatched tools', message)
        self.assertFalse(matching_identity(self.alpha, native))

    def test_all_required_fields_still_checked(self):
        for field in self.alpha:
            with self.subTest(field=field):
                changed = dict(self.alpha)
                del changed[field]
                self.assertFalse(matching_identity(self.alpha, changed))
                self.assertIn(field, identity_mismatch_message('exporter', self.alpha, changed))
                self.assertIn(field, identity_mismatch_message('exporter', changed, self.alpha))

    def test_combined_mismatch_not_hidden_by_channel(self):
        native = dict(self.alpha, channel='Experimental', source_revision='different')
        message = identity_mismatch_message('exporter', self.alpha, native)
        self.assertIn('source_revision', message)
        self.assertIn('--channel Alpha', message)
        self.assertIn('unchanged source/version', message)

    def test_real_builder_stops_before_native_build_with_actionable_advice(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            sdk, cache = root / 'sdk', root / 'cache'
            cache.mkdir()
            required = ['build-tools/36.0.0/' + tool for tool in
                        ('aapt2', 'd8', 'apksigner', 'zipalign')]
            required += ['platforms/android-36/android.jar',
                         'ndk/28.2.13676358/build/cmake/android.toolchain.cmake']
            for name in required:
                path = sdk / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            args, parser = parse_args(['game.judasproj', '--output', str(root / 'game.apk'),
                                       '--sdk', str(sdk), '--cache', str(cache), '--abis', 'arm64-v8a'])
            native = dict(self.alpha, channel='Experimental')
            error = io.StringIO()
            with patch.object(build_android, 'probe_identity', return_value=self.alpha), \
                 patch.object(build_android, 'identity', return_value=native), \
                 patch.object(build_android, 'run') as run, contextlib.redirect_stderr(error):
                with self.assertRaises(SystemExit) as caught:
                    build_android.build_apk(args, parser, {}, 'test JDK', sdk, cache)
            self.assertEqual(caught.exception.code, 2)
            self.assertIn('--channel Alpha', error.getvalue())
            self.assertIn("desktop='Alpha', Android='Experimental'", error.getvalue())
            self.assertEqual(run.call_count, 1)
            self.assertNotIn('--build', run.call_args.args[0])
            self.assertFalse((root / 'game.apk').exists())

    def test_source_archive_contract(self):
        archive = {k: v for k, v in self.alpha.items() if not k.startswith('source_untracked')}
        archive['source_origin'] = 'archive'
        self.assertTrue(matching_identity(archive, dict(archive)))
        self.assertFalse(matching_identity(archive, self.alpha))

    def test_explicit_alpha_and_default_are_deliberate(self):
        args = ['game.judasproj', '--output', '/tmp/identity-only.apk']
        default, _ = parse_args(args)
        alpha, _ = parse_args(args + ['--channel', 'Alpha'])
        self.assertEqual(default.channel, 'Experimental')
        self.assertEqual(alpha.channel, 'Alpha')
        self.assertEqual(alpha.cache.parts[-1], 'Alpha')

if __name__ == '__main__':
    unittest.main()
