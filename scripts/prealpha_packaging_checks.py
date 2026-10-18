#!/usr/bin/env python3
"""Positive and adversarial SDK/Android packaging checks; no SDK, device or compiler required."""

import argparse
import contextlib
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import re
import sys
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import package_sdk
import build_android
import package_source


BUILD = {"schema": 1, "version": "0.1.0-prealpha", "maturity": "pre-alpha",
         "channel": "Experimental", "build_id": "fixture-build", "source_revision": "1" * 40,
         "source_tree": "2" * 40, "source_dirty": False, "source_fingerprint": "3" * 64,
         "source_origin": "git", "platform": "Linux", "architecture": "x86_64",
         "source_untracked_count": 0, "source_untracked_fingerprint": hashlib.sha256(b"").hexdigest(),
         "configuration": "Release", "compiler": "GNU", "compiler_version": "fixture"}


def cmake_declared_targets(root, source_overrides=None):
    """Find literal target declarations in reachable, repository-local CMake files.

    This checks the shared, cross-platform source inventory, not host-specific
    configured targets. Native configure/build checks independently verify CMake
    semantics. Keep comments and quoted/bracket text from inventing declarations.
    """
    root = root.resolve()
    overrides = source_overrides or {}
    tokens = re.compile(
        r'(?P<space>\s+)|(?P<comment>\#\[(?P<ce>=*)\[.*?\](?P=ce)\]|\#[^\n]*)'
        r'|(?P<bracket>\[(?P<be>=*)\[.*?\](?P=be)\])'
        r'|(?P<quoted>"(?:\\.|[^"\\])*")|(?P<open>\()|(?P<close>\))'
        r'|(?P<word>[^\s()"#]+)', re.DOTALL)
    pending = [root / "CMakeLists.txt"]
    visited, targets = set(), set()
    while pending:
        path = pending.pop().resolve()
        if path in visited:
            continue
        visited.add(path)
        source = overrides.get(path.relative_to(root).as_posix())
        if source is None:
            source = path.read_text(encoding="utf-8")
        lexed = [(m.lastgroup, m.group()) for m in tokens.finditer(source)
                 if m.lastgroup not in ("space", "comment")]
        for command, opening, argument in zip(lexed, lexed[1:], lexed[2:]):
            if command[0] != "word" or opening[0] != "open":
                continue
            if argument[0] not in ("word", "quoted"):
                continue
            name = argument[1].strip('"')
            kind = command[1].lower()
            if kind in ("add_executable", "judas_add_engine_test"):
                if re.fullmatch(r"[A-Za-z0-9_.+-]+", name):
                    targets.add(name)
            elif kind == "include":
                name = name.replace("${CMAKE_CURRENT_LIST_DIR}", str(path.parent))
                name = name.replace("${CMAKE_CURRENT_SOURCE_DIR}", str(root))
                include = (root / name).resolve()
                if "$" not in name and include.is_relative_to(root):
                    if include.is_file():
                        pending.append(include)
                    elif name.endswith(".cmake"):
                        raise ValueError(f"Missing local CMake include: {name}")
    return targets


class PackagingChecks(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="judas-package-check-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "source"
        self.binaries = self.root / "bin"
        self.output = self.root / "SDK with spaces"
        self.source.mkdir()
        self.binaries.mkdir()
        self.spec = {"schema": 1, "tools": ["judas"], "windows_tools": [],
                     "files": ["LICENSE"], "directories": ["docs/judasjs"], "current_markdown": "docs"}
        for name in ("LICENSE", "docs/JUDASJS.md", "docs/judasjs/entities.md",
                     "tools/package_sdk.py", "packaging/sdk-inventory.json"):
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture\n")
        self.inventory = self.root / "inventory.json"
        self.inventory.write_text(json.dumps(self.spec))
        self.build_info = self.root / "identity.json"
        self.build_info.write_text(json.dumps(BUILD))
        executable = self.binaries / "judas"
        # The fixture tool is independent of Judas, so the test also detects absent probes.
        executable.write_text("#!" + sys.executable + "\nprint(" + repr(json.dumps(BUILD)) + ")\n")
        executable.chmod(0o755)
        self.args = argparse.Namespace(source=self.source, bin_dir=self.binaries,
                                       output=self.output, inventory=self.inventory,
                                       build_info=self.build_info, replace=True, probe_tools=os.name != "nt")

    def test_shared_inventory_contains_every_current_public_tool(self):
        spec = package_sdk.inventory(ROOT / "packaging/sdk-inventory.json")
        targets = cmake_declared_targets(ROOT)
        missing = set(spec["tools"] + spec["windows_tools"]) - targets
        self.assertFalse(missing, f"SDK tools missing reachable CMake declarations: {sorted(missing)}")
        self.assertTrue({"judas_cook", "judas_script_check", "judas_save", "judas_server",
                         "judas_volume_author", "judas_collision_cook"} <= set(spec["tools"]))
        for name in spec["files"] + spec["directories"]:
            self.assertTrue((ROOT / name).exists(), name)

    def test_inventory_detects_removed_helper_target_and_unreachable_module(self):
        source = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        missing_helper, count = re.subn(
            r"judas_add_engine_test\(judas_windows_readiness_tests[^\n]*", "", source)
        self.assertEqual(count, 1, "negative control must remove the real helper invocation")
        self.assertNotIn("judas_windows_readiness_tests",
                         cmake_declared_targets(ROOT, {"CMakeLists.txt": missing_helper}))
        missing_include = source.replace("include(cmake/targets/RenderingAssets.cmake)", "")
        self.assertNotEqual(source, missing_include, "negative control must remove the real include")
        self.assertNotIn("judas_cook",
                         cmake_declared_targets(ROOT, {"CMakeLists.txt": missing_include}))

    def test_inventory_follows_includes_without_accepting_inert_text(self):
        cmake_root = self.root / "cmake-fixture"
        (cmake_root / "nested").mkdir(parents=True)
        (cmake_root / "CMakeLists.txt").write_text('''
# add_executable(commented)
#[=[
add_executable(bracket_commented)
]=]
message("quoted\nadd_executable(quoted_target)")
message([==[\nadd_executable(bracket_target)\n]==])
include(
    "nested/tools.cmake")
''')
        (cmake_root / "nested/tools.cmake").write_text('''
include("${CMAKE_CURRENT_LIST_DIR}/../CMakeLists.txt")
add_executable(
    "real_tool" main.cpp)
judas_add_engine_test(real_test tests.cpp)
add_executable(${target} ${sources})
''')
        self.assertEqual(cmake_declared_targets(cmake_root), {"real_tool", "real_test"})
        with self.assertRaisesRegex(ValueError, "Missing local CMake include"):
            cmake_declared_targets(cmake_root, {"CMakeLists.txt": "include(missing.cmake)"})

    def test_windows_drive_relative_manifest_paths_are_rejected_on_all_hosts(self):
        for name in ("C:outside.txt", "D:/outside.txt"):
            with self.assertRaisesRegex(ValueError, "Unsafe"):
                package_sdk.relative_path(name)

    def test_stage_verify_relocate_and_deterministic_manifest(self):
        package_sdk.stage(self.args)
        first = (self.output / "SDK_MANIFEST.json").read_bytes()
        package_sdk.stage(self.args)
        self.assertEqual(first, (self.output / "SDK_MANIFEST.json").read_bytes())
        moved = self.root / "Moved Ω SDK"
        self.output.rename(moved)
        result = package_sdk.verify(moved)
        self.assertEqual(result["build"], BUILD)
        self.assertEqual(result["tools"], ["judas"])

    def test_missing_required_tool_rejected(self):
        (self.binaries / "judas").unlink()
        with self.assertRaisesRegex(ValueError, "Required SDK tool"):
            package_sdk.stage(self.args)
        self.assertFalse(self.output.exists())

    def test_windows_dependency_inventory_and_missing_dll_negative_control(self):
        windows = dict(BUILD, platform="Windows")
        self.build_info.write_text(json.dumps(windows))
        self.spec["windows_tools"] = ["judas_windows_readiness_tests"]
        self.inventory.write_text(json.dumps(self.spec))
        executable = self.binaries / "judas.exe"
        executable.write_text("#!" + sys.executable + "\nprint(" + repr(json.dumps(windows)) + ")\n")
        executable.chmod(0o755)
        (self.binaries / "judas_windows_readiness_tests.exe").write_bytes(b"fixture-only")
        for folder in ("windows-runtime", "windows-licenses"):
            (self.binaries / folder).mkdir()
        (self.binaries / "windows-runtime/required-dlls.txt").write_text("icu.dll\n")
        (self.binaries / "windows-runtime/icu.dll").write_bytes(b"fixture ICU DLL")
        (self.binaries / "windows-runtime/stale.dll").write_bytes(b"not a declared dependency")
        (self.binaries / "windows-licenses/ICU.txt").write_text("fixture notice")
        (self.binaries / "icu.dll").write_bytes(b"fixture ICU DLL")
        package_sdk.stage(self.args)
        self.assertEqual(package_sdk.verify(self.output)["tools"], ["judas.exe", "judas_windows_readiness_tests.exe"])
        self.assertFalse((self.output / "windows-runtime/stale.dll").exists())
        previous = (self.output / "SDK_MANIFEST.json").read_bytes()
        (self.binaries / "icu.dll").unlink()
        with self.assertRaises(FileNotFoundError):
            package_sdk.stage(self.args)
        self.assertEqual(previous, (self.output / "SDK_MANIFEST.json").read_bytes())

    def test_mixed_tool_identity_rejected(self):
        stale = dict(BUILD, source_fingerprint="4" * 64)
        self.args.probe_tools = True
        with patch.object(package_sdk, "probe_identity", return_value=stale) as probe:
            with self.assertRaisesRegex(ValueError, "identity mismatch"):
                package_sdk.stage(self.args)
            probe.assert_called_once()
        self.assertFalse(self.output.exists())

    def test_refuse_replacing_binary_directory_ancestor(self):
        self.args.output = self.root
        with self.assertRaisesRegex(ValueError, "separate generated directory"):
            package_sdk.stage(self.args)
        self.assertTrue((self.binaries / "judas").exists())

    def test_release_sdk_refuses_dirty_unknown_local_and_untracked_sources(self):
        self.args.release = True
        for invalid in (dict(BUILD, source_dirty=True), dict(BUILD, source_origin="unknown"),
                        dict(BUILD, channel="Local"), dict(BUILD, configuration="Debug"),
                        dict(BUILD, source_untracked_count=1)):
            self.build_info.write_text(json.dumps(invalid))
            with self.assertRaisesRegex(ValueError, "Release SDK requires"):
                package_sdk.stage(self.args)
        self.build_info.write_text(json.dumps(BUILD))
        package_sdk.stage(self.args)
        self.assertEqual(package_sdk.verify(self.output)["preset"], "release")

    def test_untracked_observations_require_complete_typed_identity(self):
        for invalid in (dict(BUILD, source_untracked_count=True), dict(BUILD, source_untracked_count=-1),
                        dict(BUILD, source_untracked_fingerprint="invented")):
            self.build_info.write_text(json.dumps(invalid))
            with self.assertRaises(ValueError):
                package_sdk.identity(self.build_info)
        incomplete = dict(BUILD)
        del incomplete["source_untracked_count"]
        self.build_info.write_text(json.dumps(incomplete))
        with self.assertRaises(ValueError):
            package_sdk.identity(self.build_info)

    @unittest.skipIf(os.name == "nt", "Fixture shebang executables are Linux controls; actual native probes are platform smoke")
    def test_native_probe_output_and_time_are_bounded(self):
        executable = self.binaries / "judas"
        executable.write_text("#!" + sys.executable + "\nprint('x'*20000)\n")
        with self.assertRaisesRegex(ValueError, "exceeds"):
            package_sdk.probe_identity(executable)
        executable.write_text("#!" + sys.executable + "\nimport time;time.sleep(20)\n")
        with self.assertRaisesRegex(ValueError, "timed out"):
            package_sdk.probe_identity(executable, timeout=.1)

    def test_failed_replacement_preserves_previous_sdk(self):
        package_sdk.stage(self.args)
        before = (self.output / "SDK_MANIFEST.json").read_bytes()
        (self.source / "LICENSE").unlink()
        with self.assertRaises(FileNotFoundError):
            package_sdk.stage(self.args)
        self.assertEqual(before, (self.output / "SDK_MANIFEST.json").read_bytes())
        package_sdk.verify(self.output)

    def test_refuse_unowned_destination(self):
        self.output.mkdir()
        (self.output / "game.js").write_text("Keep authored work")
        with self.assertRaisesRegex(ValueError, "no SDK manifest"):
            package_sdk.stage(self.args)
        self.assertEqual((self.output / "game.js").read_text(), "Keep authored work")

    def test_corrupt_missing_and_extra_payloads_detected(self):
        package_sdk.stage(self.args)
        (self.output / "LICENSE").write_text("tampered")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            package_sdk.verify(self.output)
        package_sdk.stage(self.args)
        (self.output / "judas").unlink()
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            package_sdk.verify(self.output)
        package_sdk.stage(self.args)
        (self.output / "extra.txt").write_text("unexpected")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            package_sdk.verify(self.output)

    def test_checksum_file_is_verified(self):
        package_sdk.stage(self.args)
        (self.output / "SHA256SUMS").write_text("wrong")
        with self.assertRaisesRegex(ValueError, "SHA256SUMS mismatch"):
            package_sdk.verify(self.output)

    def test_inventory_and_manifest_paths_reject_traversal(self):
        for name in ("../secret", "/etc/passwd", "a\\b"):
            with self.assertRaises(ValueError):
                package_sdk.relative_path(name)
        bad = copy.deepcopy(self.spec)
        bad["tools"].append("judas")
        self.inventory.write_text(json.dumps(bad))
        with self.assertRaises(ValueError):
            package_sdk.stage(self.args)

    def test_android_development_defaults_are_project_owned(self):
        args, _ = build_android.parse_args(["My_Game.judasproj", "--output", "game.apk"])
        self.assertEqual(args.id, "org.projectjudas.my_game")
        self.assertEqual(args.name, "My Game")
        self.assertEqual(args.version_name, "development")
        self.assertEqual(args.cache, ROOT / ".cache/android/Experimental")

    def test_android_release_requires_explicit_version_and_signing(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            build_android.parse_args(["game.judasproj", "--output", "game.apk", "--preset", "release"])
        key = self.root / "private.p12"
        key.write_bytes(b"fixture: not a usable signing key")
        command = ["game.judasproj", "--output", "game.apk", "--preset", "release", "--id", "com.example.game",
                   "--name", "Game", "--version-code", "2", "--version-name", "1.1", "--keystore", str(key), "--key-alias", "release"]
        with patch.dict(os.environ, {"JUDAS_SIGNING_PASSWORD": ""}), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            build_android.parse_args(command)
        with patch.dict(os.environ, {"JUDAS_SIGNING_PASSWORD": "test-secret-not-recorded"}):
            args, _ = build_android.parse_args(command)
        self.assertEqual(args.preset, "release")
        self.assertEqual(args.channel, "Experimental")
        self.assertEqual(args.version_name, "1.1")

    def test_android_bad_options_fail_before_build(self):
        for extra in (["--version-code", "0"], ["--version-code", "2100000001"],
                      ["--abis", "arm64-v8a,arm64-v8a"], ["--abis", "not-an-abi"], ["--jobs", "0"]):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                build_android.parse_args(["game.judasproj", "--output", "game.apk"] + extra)

    def test_android_cross_abi_identity_excludes_architecture_but_not_source(self):
        self.assertTrue(build_android.matching_identity(dict(BUILD, architecture="aarch64"), BUILD))
        self.assertFalse(build_android.matching_identity(dict(BUILD, channel="main"), BUILD))
        self.assertFalse(build_android.matching_identity(dict(BUILD, source_fingerprint="4" * 64), BUILD))
        self.assertFalse(build_android.matching_identity(dict(BUILD, source_untracked_count=1), BUILD))
        self.assertFalse(build_android.matching_identity(dict(BUILD, source_untracked_fingerprint="4" * 64), BUILD))
        incomplete = dict(BUILD)
        del incomplete["source_untracked_fingerprint"]
        self.assertFalse(build_android.matching_identity(incomplete, BUILD))

    def test_generated_zip_entries_have_stable_bytes_and_epoch(self):
        payload = self.root / "payload"
        payload.write_text("same bytes")
        outputs = [self.root / "one.zip", self.root / "two.zip"]
        with patch.dict(os.environ, {"SOURCE_DATE_EPOCH": "315532800"}):
            for path in outputs:
                with zipfile.ZipFile(path, "w") as archive:
                    build_android.zip_file(archive, payload, "payload")
        self.assertEqual(outputs[0].read_bytes(), outputs[1].read_bytes())
        with zipfile.ZipFile(outputs[0]) as archive:
            self.assertEqual(archive.getinfo("payload").date_time, (1980, 1, 1, 0, 0, 0))

    def source_checkout(self):
        for name, content in {"CMakeLists.txt": "cmake_minimum_required(VERSION 3.20)\n",
                              "engine-version.json": '{"schema":1,"version":"0.1.0-prealpha","maturity":"pre-alpha"}',
                              "src/code.cpp": "exact tracked source\n", "docs/evidence/preserved.txt": "unchanged history\n",
                              "projects/game/game.js": "game asset\n", ".gitattributes": "src/code.cpp export-ignore\n"}.items():
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
        subprocess.run(["git", "init", "-q", "--initial-branch=Experimental", str(self.source)], check=True)
        subprocess.run(["git", "-C", str(self.source), "add", "."], check=True)
        subprocess.run(["git", "-C", str(self.source), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                        "-c", "commit.gpgsign=false", "commit", "-qm", "Fixture"], check=True)
        return argparse.Namespace(source=self.source, output=self.root / "engine.tar.gz", profile="engine",
                                  channel=None, allow_untracked=False, replace=True)

    def test_source_archive_exact_bytes_provenance_and_determinism(self):
        args = self.source_checkout()
        first = package_source.archive(args)
        original = args.output.read_bytes()
        package_source.archive(args)
        self.assertEqual(original, args.output.read_bytes())
        with tarfile.open(args.output, "r:gz") as archive:
            names = archive.getnames()
            self.assertIn("src/code.cpp", names)  # export-ignore must not silently remove build input
            self.assertNotIn("docs/evidence/preserved.txt", names)
            self.assertNotIn("projects/game/game.js", names)
            info = json.load(archive.extractfile("JUDAS_SOURCE_INFO.json"))
            raw = archive.extractfile("JUDAS_SOURCE_FILES.json").read()
            self.assertEqual(info["source_files_sha256"], hashlib.sha256(raw).hexdigest())
            self.assertEqual(info["source_fingerprint"], hashlib.sha256((info["tree"] + "\n").encode()).hexdigest())
            for record in json.loads(raw)["files"]:
                payload = archive.extractfile(record["path"]).read()
                self.assertEqual(record["sha256"], hashlib.sha256(payload).hexdigest())
                expected = subprocess.check_output(["git", "-C", str(self.source), "show", "HEAD:" + record["path"]])
                self.assertEqual(payload, expected)
        self.assertEqual(first["source"]["channel"], "Experimental")

    def test_source_archive_refuses_dirty_or_unacknowledged_untracked_inputs(self):
        args = self.source_checkout()
        (self.source / "src/code.cpp").write_text("unreviewed change")
        with self.assertRaisesRegex(ValueError, "Tracked source is dirty"):
            package_source.archive(args)
        subprocess.run(["git", "-C", str(self.source), "restore", "src/code.cpp"], check=True)
        (self.source / "untracked.txt").write_text("not implicitly published")
        with self.assertRaisesRegex(ValueError, "Untracked files exist"):
            package_source.archive(args)
        args.allow_untracked = True
        result = package_source.archive(args)
        self.assertEqual(result["untrackedExcluded"], 1)
        with tarfile.open(args.output, "r:gz") as archive:
            self.assertNotIn("untracked.txt", archive.getnames())

    def test_full_source_profile_is_explicit(self):
        args = self.source_checkout()
        args.profile = "full"
        package_source.archive(args)
        with tarfile.open(args.output, "r:gz") as archive:
            self.assertIn("docs/evidence/preserved.txt", archive.getnames())
            self.assertIn("projects/game/game.js", archive.getnames())

    def test_source_archive_refuses_checkout_output_and_unapproved_companion_overwrite(self):
        args = self.source_checkout()
        args.output = self.source / "LICENSE"
        before = (self.source / "LICENSE").read_bytes()
        with self.assertRaisesRegex(ValueError, "outside the source checkout"):
            package_source.archive(args)
        self.assertEqual((self.source / "LICENSE").read_bytes(), before)
        args.output = self.root / "fresh.tar.gz"
        args.replace = False
        receipt = args.output.with_name(args.output.name + ".build.json")
        receipt.write_text("preserve this existing receipt")
        with self.assertRaisesRegex(ValueError, "receipt/checksum already exists"):
            package_source.archive(args)
        self.assertEqual(receipt.read_text(), "preserve this existing receipt")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(PackagingChecks))
    report = {"schema": 1, "suite": "prealpha-packaging", "tests": result.testsRun,
              "failures": len(result.failures), "errors": len(result.errors), "passed": result.wasSuccessful(),
              "skipped": len(result.skipped), "executed": result.testsRun - len(result.skipped),
              "scope": "Python fixture staging/negative controls; not native Windows or Android execution"}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n")
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
