import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import workspace_storage as storage


class StorageTests(unittest.TestCase):
    def test_budget_failure_preserves_content(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / '.cache').mkdir()
            payload = root / '.cache' / 'unique-source.cpp'
            payload.write_bytes(b'x' * 8192)
            policy = dict(directory_limits_bytes={'.cache': 1, 'build': 100000}, total_limit_bytes=100000)
            with self.assertRaises(ValueError):
                storage.check(root, policy)
            self.assertEqual(payload.read_bytes(), b'x' * 8192)

    def test_symlink_root_is_rejected_without_touching_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'outside').mkdir()
            (root / '.cache').symlink_to(root / 'outside', target_is_directory=True)
            with self.assertRaises(ValueError):
                storage.check(root)
            self.assertTrue((root / 'outside').is_dir())

    def test_nested_symlink_is_not_traversed_and_hardlink_counted_once(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'data').mkdir()
            (root / 'outside').mkdir()
            (root / 'outside' / 'large').write_bytes(b'x' * 65536)
            payload = root / 'data' / 'file'
            payload.write_bytes(b'x' * 8192)
            before = storage.usage(root / 'data')
            os.link(payload, root / 'data' / 'alias')
            (root / 'data' / 'external').symlink_to(root / 'outside', target_is_directory=True)
            self.assertLess(storage.usage(root / 'data') - before, 8192)

    def test_refuses_policy_path_escape(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):
                storage.check(Path(tmp), dict(directory_limits_bytes={'../outside': 10}, total_limit_bytes=10))

    def test_monitor_stops_producer_on_budget(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            command = [sys.executable, '-c',
                       "from pathlib import Path; import time; Path('.cache').mkdir(); "
                       "Path('.cache/owned').write_bytes(b'x'*65536); time.sleep(20)"]
            from unittest.mock import patch
            original = storage.check
            policy = dict(directory_limits_bytes={'.cache': 32768, 'build': 100000}, total_limit_bytes=100000)
            with patch.object(storage, 'check', side_effect=lambda r: original(r, policy)):
                with self.assertRaises(ValueError):
                    storage.run(command, root, interval=.05)
            self.assertTrue((root / '.cache' / 'owned').exists())


if __name__ == '__main__':
    unittest.main()
