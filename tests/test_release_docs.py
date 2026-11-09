"""Offline controls for version and disposition drift in current summaries."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
SPEC = importlib.util.spec_from_file_location('release_docs', ROOT / 'tools/release_docs.py')
docs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(docs)


class ReleaseDocumentationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = {'schema': 1, 'version': '1.2.5-alpha.1',
                       'release_name': 'Judas Engine Alpha v1.2.5'}
        self.publication = {'channel': 'Alpha', 'version': '1.2.5-alpha.1',
                            'tag': 'v1.2.5-alpha.1', 'source_revision': 'a' * 40,
                            'published_at': '2026-11-04T02:05:49Z',
                            'preparation_run': 123, 'publication_run': 456}
        for page in docs.PAGES:
            path = self.root / page
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('# Current\n' + docs.START + '\n' + docs.END +
                            '\nHistorical Alpha v1.0.0 stays unchanged.\n', encoding='utf-8')
        (self.root / 'docs/CURRENT_FINDINGS.md').write_text(
            '# Findings\n<!-- judas-findings:start -->\n<!-- judas-findings:end -->\n',
            encoding='utf-8')
        (self.root / 'docs/ALPHA_V1_2_5_NOTES.md').write_text('# Notes\n', encoding='utf-8')
        self.findings = {'findings': [{'id': 'DX-01', 'status': 'improved',
                                      'disposition': 'Current workflow is exercised.'}]}
        self.write_records()
        docs.refresh(self.root, write=True)

    def write_records(self):
        for name, value in [('engine-version.json', self.source),
                            ('docs/RELEASE_STATUS.json', {'schema': 1, 'publication': self.publication}),
                            ('docs/CURRENT_FINDINGS.json', self.findings)]:
            (self.root / name).write_text(json.dumps(value), encoding='utf-8')

    def test_current_source_and_publication_pass_without_git(self):
        self.assertEqual(docs.refresh(self.root), [])

    def test_version_bump_requires_all_summaries_then_refreshes(self):
        self.source.update(version='1.2.6-alpha.1', release_name='Judas Engine Alpha v1.2.6')
        (self.root / 'docs/ALPHA_V1_2_6_NOTES.md').write_text('# Next notes\n', encoding='utf-8')
        self.write_records()
        self.assertEqual(set(docs.refresh(self.root)), set(docs.PAGES))
        docs.refresh(self.root, write=True)
        self.assertEqual(docs.refresh(self.root), [])
        text = (self.root / 'docs/RELEASING.md').read_text(encoding='utf-8')
        self.assertIn('Source identity is **Judas Engine Alpha v1.2.6**', text)
        self.assertIn('v1.2.5-alpha.1', text)  # Preparation must not claim publication.

    def test_historical_prose_is_preserved(self):
        before = (self.root / 'README.md').read_text(encoding='utf-8')
        docs.refresh(self.root, write=True)
        self.assertEqual((self.root / 'README.md').read_text(encoding='utf-8'), before)

    def test_manually_stale_summary_is_detected(self):
        path = self.root / 'docs/ALPHA_SUPPORT_MATRIX.md'
        path.write_text(path.read_text(encoding='utf-8').replace('1.2.5', '1.2.2'), encoding='utf-8')
        self.assertEqual(docs.refresh(self.root), ['docs/ALPHA_SUPPORT_MATRIX.md'])

    def test_published_receipt_change_updates_release_page(self):
        self.publication['publication_run'] = 789
        self.write_records()
        self.assertEqual(docs.refresh(self.root), ['docs/RELEASING.md'])

    def test_finding_change_requires_matching_table(self):
        self.findings['findings'][0]['disposition'] = 'Corrected with scoped evidence.'
        self.write_records()
        self.assertEqual(docs.refresh(self.root), ['docs/CURRENT_FINDINGS.md'])
        docs.refresh(self.root, write=True)
        self.assertEqual(docs.refresh(self.root), [])

    def test_missing_and_duplicate_markers_reject_without_partial_write(self):
        path = self.root / 'docs/ANDROID.md'
        before = (self.root / 'README.md').read_bytes()
        for contents in ('# No managed block', docs.START + docs.START + docs.END):
            path.write_text(contents, encoding='utf-8')
            with self.assertRaises(ValueError):
                docs.refresh(self.root, write=True)
            self.assertEqual((self.root / 'README.md').read_bytes(), before)

    def test_wrong_tag_commit_and_run_ids_are_rejected(self):
        for key, invalid in [('tag', 'v1.2.2-alpha.1'), ('source_revision', 'main'),
                             ('publication_run', 0), ('preparation_run', True)]:
            with self.subTest(key=key):
                prior = self.publication[key]
                self.publication[key] = invalid
                self.write_records()
                with self.assertRaises(ValueError):
                    docs.refresh(self.root)
                self.publication[key] = prior

    def test_missing_notes_and_newer_publication_are_rejected(self):
        (self.root / 'docs/ALPHA_V1_2_5_NOTES.md').unlink()
        with self.assertRaisesRegex(ValueError, 'missing versioned release notes'):
            docs.refresh(self.root)
        self.publication.update(version='1.2.6-alpha.1', tag='v1.2.6-alpha.1')
        self.write_records()
        with self.assertRaisesRegex(ValueError, 'newer than'):
            docs.refresh(self.root)

    def test_cli_nonzero_for_drift_and_write_repairs(self):
        path = self.root / 'README.md'
        path.write_text(path.read_text(encoding='utf-8').replace('1.2.5', '1.2.1'), encoding='utf-8')
        command = [sys.executable, str(ROOT / 'tools/release_docs.py'), '--root', str(self.root)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn('--write', result.stdout)
        result = subprocess.run(command + ['--write'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)

    def test_release_workflows_keep_the_guard_before_build(self):
        workflow = (ROOT / '.github/workflows/release-candidate.yml').read_text(encoding='utf-8')
        self.assertEqual(workflow.count('name: Verify release and status documentation before building'), 2)
        self.assertLess(workflow.index('run: python3 tools/release_docs.py'), workflow.index('cmake -S'))
        self.assertLess(workflow.index('run: python tools/release_docs.py'),
                        workflow.index('name: Build complete native MSVC SDK'))
        current = (ROOT / '.github/workflows/current-docs.yml').read_text(encoding='utf-8')
        self.assertIn("'engine-version.json'", current)
        self.assertIn('test_release_docs.py', current)


if __name__ == '__main__':
    unittest.main()
