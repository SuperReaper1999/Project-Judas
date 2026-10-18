"""Negative controls for the focused gate, not native engine certification."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('gate', ROOT / 'scripts/prealpha_validate.py')
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


class GateTests(unittest.TestCase):
    def test_exit_and_timeout_controls(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for code, expected in [('pass', 'passed'), ('raise SystemExit(7)', 'failed'),
                                   ('import time;time.sleep(10)', 'timeout')]:
                result = gate.run_case([sys.executable, '-c', code], root / 'log', root,
                                       os.environ.copy(), .1 if expected == 'timeout' else 5)
                self.assertEqual(result['status'], expected)
                self.assertEqual(len(result['binarySha256']), 64)
            result = gate.run_case([str(root / 'absent')], root / 'missing', root, os.environ.copy(), 5)
            self.assertEqual(result['status'], 'unavailable')

    def test_manifest_rejects_duplicates_and_bad_targets(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'manifest.json'
            entry = dict(name='probe', target='judas_probe_tests', groups=['core'])
            for entries in [[entry, entry], [dict(entry, target='../unrelated')]]:
                path.write_text(json.dumps(dict(format=1, tests=entries)))
                with self.assertRaises(ValueError):
                    gate.load_manifest(path)

    def test_deferred_findings_are_not_tests(self):
        manifest = gate.load_manifest(gate.MANIFEST)
        self.assertTrue(manifest['tests'])
        findings = {row['id']: row for row in manifest['known_findings']}
        # Resolved contracts may graduate into the ordinary gate. Unresolved
        # findings must not silently become acceptance passes through scheduling.
        scheduled = {key for key, row in findings.items() if row['scheduled']}
        self.assertEqual(scheduled, {'CCD-MESH-ARRIVAL', 'LIQUID-BAKE-ROTATION', 'LIQUID-BAKE-KNOTS'})
        self.assertEqual(findings['LIQUID-ORIGIN-GRID-QUERY']['status'], 'open-query-continuity-failing')
        knots = next(row for row in manifest['tests'] if row['name'] == 'liquid-bake-knots-only')
        self.assertEqual(knots['args'][:2], ['--bake-only', 'knots'])
        self.assertEqual(knots['groups'], ['liquid'])
        full = next(row for row in manifest['tests'] if row['name'] == 'liquid-origin-grid-query')
        self.assertEqual(full['args'][:2], ['--bake-regression', 'knots'])
        self.assertTrue(full['manual'])
        self.assertEqual(findings['CCD-MESH-ARRIVAL']['status'], 'resolved-first-arrival')
        arrival = next(row for row in manifest['tests'] if row['name'] == 'impact-mesh-arrival')
        self.assertEqual(arrival['groups'], ['collision'])
        self.assertFalse(arrival.get('manual', False))
        for row in manifest['tests']:
            if row.get('finding') in findings and not findings[row['finding']]['scheduled']:
                self.assertTrue(row.get('manual', False), row['name'])
                self.assertEqual(row['groups'], ['known-failures'], row['name'])
        self.assertTrue(all(row.get('manual', False) for row in manifest['tests']
                            if 'known-failures' in row['groups']))
        self.assertNotIn('FLU-001', [row['name'] for row in manifest['tests']])

    def test_stale_or_absent_embedded_identity_cannot_pass(self):
        expected = dict(source_revision='a' * 40, source_fingerprint='b' * 64, source_dirty=False)
        current = dict(schema=1, **expected)
        self.assertEqual(gate.validate_provenance(current, expected), current)
        for altered in [{}, dict(current, source_revision='c' * 40),
                        dict(current, source_fingerprint='d' * 64), dict(current, source_dirty=True)]:
            with self.assertRaises(ValueError):
                gate.validate_provenance(altered, expected)

    def test_identity_probe_is_bound_to_test_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / 'probe'
            executable.write_bytes(b'original')
            expected = dict(source_revision='a' * 40)
            def replaced(*args, **kwargs):
                executable.write_bytes(b'replacement')
                return dict(schema=1, **expected)
            with mock.patch.object(gate, 'probe_identity', side_effect=replaced):
                with self.assertRaisesRegex(ValueError, 'during its identity probe'):
                    gate.run_verified_case([str(executable)], root / 'log', root,
                                           os.environ.copy(), 5, expected)
            result = gate.run_case([str(executable)], root / 'log', root,
                                   os.environ.copy(), 5, expected_sha256='0' * 64)
            self.assertEqual(result['status'], 'unavailable')
            self.assertIn('after its identity probe', result['error'])

    def test_archive_added_input_cannot_certify_an_old_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            source = root / 'src/old.cpp'
            source.write_text('old')
            manifest = root / 'JUDAS_SOURCE_FILES.json'
            manifest.write_text(json.dumps(dict(files=[dict(path='src/old.cpp', sha256=gate.digest(source))])))
            info = dict(source_files_sha256=gate.digest(manifest), revision='a' * 40,
                        source_fingerprint='b' * 64, source_dirty=False)
            (root / 'JUDAS_SOURCE_INFO.json').write_text(json.dumps(info))
            with mock.patch.object(gate, 'ROOT', root), mock.patch.object(gate, 'git', return_value=None):
                self.assertFalse(gate.source_identity()['source_dirty'])
                (root / 'src/new.h').write_text('new')
                with self.assertRaisesRegex(ValueError, 'added build input'):
                    gate.source_identity()
                (root / 'src/new.h').unlink()
                (root / 'third_party').mkdir()
                (root / 'third_party/compiled.cpp').write_text('new native input')
                with self.assertRaisesRegex(ValueError, 'added build input'):
                    gate.source_identity()


if __name__ == '__main__':
    unittest.main()
