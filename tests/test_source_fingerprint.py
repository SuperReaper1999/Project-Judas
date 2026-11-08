"""Provenance compatibility against the previous CMake hash calculation."""

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("source_fingerprint", ROOT / "tools/source_fingerprint.py")
source_fingerprint = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source_fingerprint)


@unittest.skipUnless(shutil.which("git") and shutil.which("cmake"), "Git and CMake required")
class FingerprintTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="judas-provenance-")
        self.root = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Judas test")
        self.git("config", "user.email", "test@example.invalid")
        (self.root / "source.cpp").write_text("initial\n", encoding="utf-8")
        (self.root / "binary.dat").write_bytes(bytes(range(256)))
        self.git("add", ".")
        self.git("commit", "-qm", "fixture")
        self.tree = self.git("rev-parse", "HEAD^{tree}").strip()

    def tearDown(self):
        self.temp.cleanup()

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args], text=True)

    def compare(self, dirty):
        # This is the actual former CMake implementation, not a second Python
        # implementation of the candidate. The fixture stays intentionally small.
        script = self.root / "oracle.cmake"
        script.write_text(
            f'execute_process(COMMAND git -C "{self.root.as_posix()}" diff HEAD --binary '
            '--no-ext-diff --full-index --no-color --no-renames --no-textconv '
            '--diff-algorithm=myers OUTPUT_VARIABLE diff RESULT_VARIABLE result)\n'
            'if(NOT result EQUAL 0)\nmessage(FATAL_ERROR "diff failed")\nendif()\n'
            f'string(SHA256 hash "{self.tree}\\n${{diff}}")\n'
            'message("ORACLE=${hash}")\n', encoding="utf-8")
        run = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True, check=True)
        expected = next(line.removeprefix("ORACLE=") for line in run.stderr.splitlines()
                        if line.startswith("ORACLE="))
        actual = source_fingerprint.fingerprint("git", self.root, self.tree)
        self.assertEqual(actual["source_fingerprint"], expected)
        self.assertEqual(actual["source_dirty"], dirty)
        return actual

    def test_clean_and_untracked_are_separate(self):
        before = self.compare(False)
        (self.root / "untracked.cpp").write_text("not yet a tracked input\n")
        self.assertEqual(self.compare(False), before)

    def test_text_binary_and_staged_deletion(self):
        (self.root / "source.cpp").write_text("new Unicode: terrain 🌍\n", encoding="utf-8")
        first = self.compare(True)
        (self.root / "binary.dat").write_bytes(bytes(range(255, -1, -1)))
        self.assertNotEqual(self.compare(True), first)
        (self.root / "source.cpp").unlink()
        self.git("add", "-u")
        self.compare(True)

    def test_carriage_returns_match_cmake_capture(self):
        (self.root / "source.cpp").write_bytes(b"new\r\nsecond\rline\n")
        self.compare(True)

    def test_failed_git_cannot_publish_an_identity(self):
        with self.assertRaises(RuntimeError):
            source_fingerprint.fingerprint("git", self.root / "missing", self.tree)


if __name__ == "__main__":
    unittest.main()
