import ast
import copy
import fnmatch
import importlib.util
from pathlib import Path
import re
import unittest
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('promotion', ROOT/'scripts/check_ci_promotion.py')
promotion = importlib.util.module_from_spec(spec)
spec.loader.exec_module(promotion)


def path_patterns(workflow, key):
    # These workflow fields are quoted flow sequences, valid YAML and Python
    # string lists. No hosted YAML parser dependency is needed for this policy.
    text = (ROOT/'.github/workflows'/workflow).read_text(encoding='utf-8')
    match = re.search(r'^\s+'+key+r':\s*(\[[\s\S]*?\])', text, re.MULTILINE)
    if not match:
        raise AssertionError('Missing quoted path policy: '+workflow+'/'+key)
    return ast.literal_eval(match.group(1))


class Promotion(unittest.TestCase):
    def setUp(self):
        self.repository = 'owner/engine'
        self.revision = 'a'*40
        self.run = dict(head_sha=self.revision, conclusion='success',
                        head_repository={'full_name': self.repository},
                        head_branch='Experimental', event='push',
                        path='.github/workflows/prealpha.yml')
        self.jobs = [dict(name='linux ('+c+')', status='completed', conclusion='success')
                     for c in ('Alpha', 'Experimental')]

    def accepted(self, run=None, jobs=None):
        return promotion.qualifies(run or self.run, self.jobs if jobs is None else jobs,
                                   self.repository, self.revision)

    def test_same_source_both_native_channels_are_reusable(self):
        self.assertTrue(self.accepted())

    def test_missing_skipped_failed_or_incomplete_channels_are_not_proof(self):
        self.assertFalse(self.accepted(jobs=[]))
        self.assertFalse(self.accepted(jobs=self.jobs[:1]))
        for field, value in [('conclusion', 'skipped'), ('conclusion', 'failure'), ('status', 'in_progress')]:
            jobs = copy.deepcopy(self.jobs);jobs[1][field] = value
            self.assertFalse(self.accepted(jobs=jobs))
        self.assertFalse(self.accepted(jobs=[dict(name='tooling', status='completed', conclusion='success')]))

    def test_other_source_repository_event_branch_or_workflow_is_rejected(self):
        for key, value in [('head_sha', 'b'*40), ('head_repository', {'full_name': 'other/engine'}),
                           ('conclusion', 'failure'), ('head_branch', 'main'),
                           ('event', 'pull_request'), ('path', '.github/workflows/ci-tooling.yml')]:
            run = copy.deepcopy(self.run);run[key] = value
            self.assertFalse(self.accepted(run=run))

    def test_release_requires_both_platform_jobs_and_dispatch(self):
        run = dict(self.run, path='.github/workflows/release-candidate.yml', event='workflow_dispatch')
        jobs = [dict(name=n, status='completed', conclusion='success') for n in ('linux', 'windows')]
        self.assertTrue(self.accepted(run, jobs))
        self.assertFalse(self.accepted(run, jobs[:1]))
        self.assertFalse(self.accepted(dict(run, event='push'), jobs))


class Routing(unittest.TestCase):
    def test_release_freezes_strict_source_before_running_qualification(self):
        workflow=(ROOT/'.github/workflows/release-candidate.yml').read_text(encoding='utf-8')
        self.assertLess(workflow.index('Snapshot exact tracked source'),workflow.index('scripts/qualify_m100.py'))
        self.assertEqual(workflow.count('python3 tools/package_source.py'),1)
        self.assertNotIn('--allow-untracked',workflow)
        self.assertIn('git diff --exit-code HEAD',workflow)

    def test_native_and_unknown_inputs_still_require_full_gates(self):
        ignored = path_patterns('prealpha.yml', 'paths-ignore')
        for name in ('src/PhysicsWorld.h', 'src/RendererWeather.inl', 'CMakeLists.txt',
                     'cmake/Physics.cmake', 'tools/package_sdk.py', 'tools/validation/suites.json',
                     'docs/judas.d.ts', 'tests/NativeTests.cpp', 'tests/fixtures/input.json',
                     'projects/game/Assets/player.js', 'tools/build_android.py',
                     'tests/test_android_orientation.py',
                     'platform/android/java/org/projectjudas/runtime/JudasActivity.java',
                     'unknown/future-file'):
            self.assertFalse(any(fnmatch.fnmatchcase(name, p) for p in ignored), name)

    def test_tooling_exclusions_have_a_lightweight_validation_route(self):
        ignored = path_patterns('prealpha.yml', 'paths-ignore')
        tooling = path_patterns('ci-tooling.yml', 'paths')
        docs = {'docs/evidence/**', 'docs/**/*.md', 'docs/*.txt', '*.md'}
        # Every native-gate exclusion needs a lightweight route. Tooling may
        # additionally cover native inputs; that overlap must not disable gates.
        self.assertLessEqual(set(ignored)-docs, set(tooling))
        for name in ('scripts/qualify_m100.py', 'tools/package_source.py', 'tests/test_release_packaging.py',
                     '.github/workflows/release-candidate.yml', '.gitignore'):
            self.assertTrue(any(fnmatch.fnmatchcase(name, p) for p in ignored))
            self.assertTrue(any(fnmatch.fnmatchcase(name, p) for p in tooling))

    def test_android_inputs_receive_both_tooling_and_native_validation(self):
        ignored = path_patterns('prealpha.yml', 'paths-ignore')
        tooling = path_patterns('ci-tooling.yml', 'paths')
        for name in ('tools/build_android.py', 'tests/test_android_orientation.py',
                     'platform/android/java/org/projectjudas/runtime/JudasActivity.java'):
            self.assertTrue(any(fnmatch.fnmatchcase(name, p) for p in tooling), name)
            self.assertFalse(any(fnmatch.fnmatchcase(name, p) for p in ignored), name)

    def assert_mutated_route_rejected(self, workflow, old, new, validator):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); destination = root/'.github/workflows'
            destination.mkdir(parents=True)
            for name in ('prealpha.yml', 'ci-tooling.yml'):
                text = (ROOT/'.github/workflows'/name).read_text(encoding='utf-8')
                if name == workflow:
                    self.assertIn(old, text)
                    text = text.replace(old, new, 1)
                (destination/name).write_text(text, encoding='utf-8')
            with patch.dict(globals(), ROOT=root), self.assertRaises(AssertionError):
                validator()

    def test_missing_lightweight_route_is_rejected(self):
        self.assert_mutated_route_rejected('ci-tooling.yml', "'tools/package_source.py',", '',
            self.test_tooling_exclusions_have_a_lightweight_validation_route)

    def test_excluding_android_native_validation_is_rejected(self):
        self.assert_mutated_route_rejected('prealpha.yml', "'scripts/check_ci_promotion.py',",
            "'tools/build_android.py', 'scripts/check_ci_promotion.py',",
            self.test_android_inputs_receive_both_tooling_and_native_validation)


if __name__ == '__main__':
    unittest.main()
