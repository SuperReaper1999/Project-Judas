"""Protect deterministic frame telemetry from display wrapper diagnostics."""

import hashlib
import os
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.vulkan.compare_backends import run


@unittest.skipUnless(os.name == "posix", "Xvfb wrapper uses POSIX redirection")
class VulkanParityTests(unittest.TestCase):
    def test_xvfb_stderr_cannot_split_csv_output(self):
        with TemporaryDirectory() as scratch:
            root = Path(scratch)
            wrapper = root / "xvfb-run"
            wrapper.write_text('#!/bin/sh\nshift 3\nexec "$@" 2>&1\n')
            wrapper.chmod(0o755)
            executable = root / "judas"
            first = "0," + "0," * 9
            last = "0," * 9 + "0"
            executable.write_text(
                "#!/bin/sh\n"
                f"printf '%s' '{first}'\n"
                "printf '%s\\n' '[Graphics] Vulkan backend: test device, validation on' >&2\n"
                f"printf '%s\\n' '{last}'\n"
            )
            executable.chmod(0o755)
            options = SimpleNamespace(frames=1, profile="portable", output=root, validation=True,
                                      device=None, xvfb=True, screen="640x480", timeout=5)
            with patch.dict(os.environ, PATH=str(root) + os.pathsep + os.environ["PATH"]):
                result = run(executable, root / "project.judasproj", [], [], root / "captures",
                             "vulkan", options)
            self.assertEqual(result["exit"], 0)
            self.assertEqual(result["simulationRows"], 1)
            self.assertEqual(result["simulationSha256"], hashlib.sha256((first + last).encode()).hexdigest())
