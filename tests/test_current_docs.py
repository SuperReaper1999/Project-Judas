"""Positive and negative controls for current-reference navigation checks."""
import importlib.util
import json
import re
import tempfile
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location('current_docs', Path(__file__).resolve().parents[1] / 'scripts/check_current_docs.py')
docs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(docs)


class CurrentDocumentationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'index.md').write_text('[Guide](guide.md#hello-world)\n')
        (self.root / 'guide.md').write_text('# Hello world\n# Hello world\n## API `thing()`\n')

    def run_check(self, text=None, tracked=None):
        if text is not None:
            (self.root / 'index.md').write_text(text)
        return docs.check(self.root, ['index.md'], tracked)

    def test_local_path_and_heading(self):
        self.assertTrue(self.run_check()['pass'])

    def test_missing_file_is_rejected(self):
        self.assertFalse(self.run_check('[bad](absent.md)')['pass'])

    def test_missing_heading_is_rejected(self):
        self.assertFalse(self.run_check('[bad](guide.md#invented)')['pass'])

    def test_fenced_examples_comments_and_remote_links_are_ignored(self):
        text = '```md\n[example](missing.md)\n```\n<!-- [comment](absent.md) -->\n[remote](https://example.invalid/anything)'
        self.assertTrue(self.run_check(text)['pass'])

    def test_second_heading_and_inline_punctuation(self):
        self.assertTrue(self.run_check('[duplicate](guide.md#hello-world-1) [api](guide.md#api-thing)')['pass'])

    def test_reference_definition_and_encoded_space(self):
        (self.root / 'a guide.md').write_text('# Guide')
        self.assertTrue(self.run_check('[read][guide]\n[guide]: <a%20guide.md#guide>')['pass'])
        self.assertFalse(self.run_check('[guide]: absent.md')['pass'])

    def test_escape_and_absolute_path_rejected(self):
        self.assertFalse(self.run_check('[bad](../elsewhere.md)')['pass'])
        self.assertFalse(self.run_check('[bad](/tmp/guide.md)')['pass'])

    def test_untracked_destination_rejected(self):
        self.assertFalse(self.run_check(tracked={'index.md'})['pass'])
        self.assertTrue(self.run_check(tracked={'index.md', 'guide.md'})['pass'])

    def test_explicit_html_anchor(self):
        (self.root / 'guide.md').write_text('<a id="explicit"></a>\n# Title')
        self.assertTrue(self.run_check('[anchor](guide.md#explicit)')['pass'])

    def test_duplicate_inventory_rejected(self):
        self.assertFalse(docs.check(self.root, ['index.md', 'index.md'])['pass'])


class MaintainedWalkthroughTests(unittest.TestCase):
    def test_beacon_grove_generator_and_packaged_readmes_match(self):
        project = docs.ROOT / 'projects/beacon_grove/README.md'
        template = docs.ROOT / 'tools/demos/beacon_grove/README.md'
        self.assertEqual(project.read_bytes(), template.read_bytes())

    def test_documented_native_case_commands_use_registered_selectors(self):
        manifest = json.loads((docs.ROOT / 'tools/validation/suites.json').read_text())
        registered = {case['name'] for case in manifest['tests']}
        for document in ('docs/CAMERA_TARGETS.md', 'docs/DEVELOPER_WORKFLOWS.md',
                         'projects/beacon_grove/README.md'):
            with self.subTest(document=document):
                text = (docs.ROOT / document).read_text()
                commands = [block for block in re.findall(r'```sh\n(.*?)```', text, re.DOTALL)
                            if 'scripts/prealpha_validate.py' in block]
                self.assertTrue(commands, 'missing native validation command')
                for block in commands:
                    selectors = set(re.findall(r'--test\s+([\w-]+)', block))
                    self.assertTrue(selectors, 'missing explicit case selector')
                    self.assertFalse(selectors - registered, selectors - registered)


if __name__ == '__main__':
    unittest.main()
