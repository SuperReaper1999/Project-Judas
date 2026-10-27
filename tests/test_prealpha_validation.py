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

    def test_manifest_rejects_malformed_or_duplicate_invocations(self):
        entry = dict(name='probe', target='judas_probe_tests', groups=['core'])
        finding = dict(id='KNOWN-FAILURE', status='deferred-failing', scheduled=False,
                       reason='A preserved contract.', evidence='docs/evidence/original')
        cases = [
            dict(tests=[]),
            dict(tests=['not an object']),
            dict(tests=[dict(entry, args='--a-string-is-not-an-argument-list')]),
            dict(tests=[dict(entry, groups='core')]),
            dict(tests=[dict(entry, groups=['core', 'core'])]),
            dict(tests=[dict(entry, manual='false')]),
            dict(tests=[entry, dict(entry, name='same-invocation-new-name')]),
            dict(tests=[dict(entry, args=['{unknown}'])]),
            dict(tests=[dict(entry, args=['{output.__class__}'])]),
            dict(tests=[dict(entry, args=['{root!r}'])]),
            dict(tests=[dict(entry, args=['{output:>10}'])]),
            dict(tests=[dict(entry, args=['{unclosed'])]),
            dict(tests=[dict(entry, groups=['known-failures'])]),
            dict(tests=[dict(entry, finding='UNKNOWN')]),
            dict(tests=[dict(entry, finding='KNOWN-FAILURE')], known_findings=[finding]),
            dict(tests=[entry], known_findings=[finding, finding]),
            dict(tests=[entry], known_findings=[dict(finding, scheduled='false')]),
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'manifest.json'
            for case in cases:
                with self.subTest(case=case):
                    path.write_text(json.dumps(dict(format=1, **case)))
                    with self.assertRaises(ValueError):
                        gate.load_manifest(path)

    def test_same_binary_different_modes_and_explicit_manual_case_remain_valid(self):
        entry = dict(name='probe', target='judas_probe_tests', groups=['core'])
        finding = dict(id='KNOWN-FAILURE', status='deferred-failing', scheduled=False,
                       reason='A preserved contract.', evidence='docs/evidence/original')
        data = dict(format=1, tests=[entry, dict(entry, name='different-mode',
                    args=['--mode', '{output}/fixture', '{root}', '{{literal}}']),
                    dict(entry, name='preserved-failure', args=['--original-failure'],
                         groups=['known-failures'], manual=True, finding='KNOWN-FAILURE')],
                    known_findings=[finding])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'manifest.json'
            path.write_text(json.dumps(data))
            self.assertEqual(gate.load_manifest(path), data)

    def test_manifest_only_cli_never_needs_a_native_build(self):
        import subprocess
        result = subprocess.run([sys.executable, str(ROOT / 'scripts/prealpha_validate.py'),
                                 '--check-manifest', '--build-dir', '/absent/native/build'],
                                text=True, capture_output=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        manifest = gate.load_manifest(gate.MANIFEST)
        self.assertEqual(summary['tests'], len(manifest['tests']))
        self.assertEqual(summary['manual'], 1)

    def test_finding_status_annotation_preserves_observed_results(self):
        manifest = gate.load_manifest(gate.MANIFEST)
        entries = {row['name']: row for row in manifest['tests']}
        for name, expected in [('impact-mesh-continuation', 'resolved-original-fixtures-bounded-ccd'),
                               ('liquid-origin-grid-query', 'open-query-continuity-failing')]:
            for observed, exit_code in [('passed', 0), ('failed', 1)]:
                with self.subTest(name=name, observed=observed):
                    result = dict(status=observed, exit=exit_code)
                    gate.annotate_finding(result, entries[name], manifest)
                    self.assertEqual(result['status'], observed)
                    self.assertEqual(result['exit'], exit_code)
                    self.assertEqual(result['findingStatus'], expected)
                    self.assertEqual(result['classification'],
                                     expected + '; observed result remains authoritative')
        result = dict(status='failed', exit=7)
        gate.annotate_finding(result, entries['character-motor'], manifest)
        self.assertEqual(result, dict(status='failed', exit=7))

    def test_deferred_findings_are_not_tests(self):
        manifest = gate.load_manifest(gate.MANIFEST)
        self.assertTrue(manifest['tests'])
        findings = {row['id']: row for row in manifest['known_findings']}
        # Resolved contracts may graduate into the ordinary gate. Unresolved
        # findings must not silently become acceptance passes through scheduling.
        scheduled = {key for key, row in findings.items() if row['scheduled']}
        self.assertEqual(scheduled, {'CCD-MESH-ARRIVAL', 'CCD-MESH-CONTINUATION', 'LIQUID-BAKE-ROTATION', 'LIQUID-BAKE-KNOTS',
                                     'LIQUID-ORIGIN-TOTAL-IDENTITY'})
        self.assertEqual(findings['LIQUID-ORIGIN-GRID-QUERY']['status'], 'open-query-continuity-failing')
        knots = next(row for row in manifest['tests'] if row['name'] == 'liquid-bake-knots-only')
        self.assertEqual(knots['args'][:2], ['--bake-only', 'knots'])
        self.assertEqual(knots['groups'], ['liquid'])
        full = next(row for row in manifest['tests'] if row['name'] == 'liquid-origin-grid-query')
        self.assertEqual(full['args'][:2], ['--bake-regression', 'knots'])
        self.assertTrue(full['manual'])
        self.assertEqual(findings['LIQUID-ORIGIN-TOTAL-IDENTITY']['status'], 'resolved-oracle-causal-baseline')
        atomic = next(row for row in manifest['tests'] if row['name'] == 'liquid-origin-total-identity')
        self.assertEqual(atomic['args'][:2], ['--bake-regression', 'normalization'])
        self.assertFalse(atomic.get('manual', False))
        controls = next(row for row in manifest['tests'] if row['name'] == 'liquid-origin-query-controls')
        self.assertEqual(controls['args'][:2], ['--query-controls', 'knots'])
        self.assertNotIn('LIQUID-ORIGIN-GRID-QUERY', controls.get('finding', ''))
        self.assertEqual(findings['CCD-MESH-ARRIVAL']['status'], 'resolved-first-arrival')
        arrival = next(row for row in manifest['tests'] if row['name'] == 'impact-mesh-arrival')
        self.assertEqual(arrival['groups'], ['collision'])
        self.assertFalse(arrival.get('manual', False))
        continuation = next(row for row in manifest['tests'] if row['name'] == 'impact-mesh-continuation')
        self.assertEqual(continuation['args'], ['60', '', '1.2'])
        self.assertEqual(continuation['groups'], ['collision'])
        self.assertFalse(continuation.get('manual', False))
        self.assertEqual(findings['CCD-MESH-CONTINUATION']['status'],
                         'resolved-original-fixtures-bounded-ccd')
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

    def test_gate_uses_writable_output_and_explicit_engine_assets(self):
        # Execute a real child requiring both the legacy relative build scratch
        # directory and an explicit asset root, without writing to source.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / 'results'
            manifest = root / 'manifest.json'
            manifest.write_text(json.dumps(dict(format=1, tests=[dict(
                name='scratch', target='judas_scratch_tests', groups=['core'])])))
            code = ("import os,pathlib; "
                    "assert pathlib.Path.cwd()!=pathlib.Path(os.environ['JUDAS_ENGINE_ROOT']); "
                    "assert (pathlib.Path(os.environ['JUDAS_ENGINE_ROOT'])/'engine-version.json').is_file(); "
                    "pathlib.Path('build/actual-child-output').write_text('ok')")
            def execute(command, log, cwd, environment, timeout, expected, **kwargs):
                return gate.run_case([sys.executable, '-c', code], log, cwd, environment, timeout)
            argv = ['gate', '--manifest', str(manifest), '--output', str(output)]
            with mock.patch.object(sys, 'argv', argv), \
                 mock.patch.object(gate, 'source_identity', return_value={}), \
                 mock.patch.object(gate, 'git', return_value=None), \
                 mock.patch.object(gate, 'run_verified_case', side_effect=execute):
                self.assertEqual(gate.main(), 0)
            self.assertEqual((output / 'build/actual-child-output').read_text(), 'ok')
            # The same child detects the missing mechanism: no scratch build.
            negative = root / 'negative'
            negative.mkdir()
            result = gate.run_case([sys.executable, '-c', code], negative / 'log', negative,
                                   dict(os.environ, JUDAS_ENGINE_ROOT=str(ROOT)), 5)
            self.assertEqual(result['status'], 'failed')

    def test_channel_policy_uses_verified_binary_and_never_hides_failure(self):
        entry = dict(name='surface', target='judas_liquid_surface_tests')
        expected = dict(source_revision='a' * 40)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for channel in ('Alpha', 'Experimental'):
                identity = dict(schema=1, channel=channel, **expected)
                with mock.patch.object(gate, 'probe_identity', return_value=identity):
                    result = gate.run_verified_case(
                        [sys.executable, '-c', 'raise SystemExit(7)'], root / 'log', root,
                        os.environ.copy(), 5, expected, entry=entry, expected_channel=channel)
                self.assertEqual(result['status'], 'not-exercised' if channel == 'Alpha' else 'failed')
                self.assertEqual(result['exit'], None if channel == 'Alpha' else 7)
            # Neither stale provenance nor the wrong compiled channel may skip.
            for identity, error in [
                (dict(schema=1, channel='Alpha', source_revision='b' * 40), 'stale'),
                (dict(schema=1, channel='Experimental', **expected), 'channel differs')]:
                with mock.patch.object(gate, 'probe_identity', return_value=identity):
                    with self.assertRaisesRegex(ValueError, error):
                        gate.run_verified_case([sys.executable], root / 'log', root,
                            os.environ.copy(), 5, expected, entry=entry, expected_channel='Alpha')

    def test_unavailable_main_receipt_is_not_a_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / 'results'
            argv = ['gate', '--test', 'liquid-surface', '--output', str(output)]
            with mock.patch.object(sys, 'argv', argv), \
                 mock.patch.object(gate, 'source_identity', return_value={}), \
                 mock.patch.object(gate, 'git', return_value=None), \
                 mock.patch.object(gate, 'run_verified_case', return_value={
                     'status': 'not-exercised', 'exit': None, 'reason': 'fixture unavailable'}):
                self.assertEqual(gate.main(), 77)
            receipt = json.loads((output / 'RESULTS.json').read_text())
            self.assertFalse(receipt['passed'])
            self.assertEqual(receipt['tests'][0]['status'], 'not-exercised')

    def test_channel_override_cannot_hide_real_execution(self):
        with mock.patch.object(sys, 'argv', ['gate', '--channel', 'Alpha', '--test', 'liquid-surface']):
            with self.assertRaises(SystemExit) as error:
                gate.main()
            self.assertEqual(error.exception.code, 2)

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
