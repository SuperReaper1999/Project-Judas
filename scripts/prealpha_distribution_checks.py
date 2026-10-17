#!/usr/bin/env python3
"""Dependency/ABI and APK-integrity positive/negative controls, without compilers."""

import argparse
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import binary_dependencies
import build_android
import inspect_sdk
import verify_android


def elf_fixture(machine=62, alignment=16384, glibc="2.39", needed="libc.so.6", required_label=None):
    """Independent, tiny ELF with imports + VERNEED + a higher VERDEF decoy."""
    provider = b"\0" + needed.encode() + b"\0"
    label = required_label or 'GLIBC_' + glibc
    strings = provider + label.encode() + b"\0GLIBC_99.0\0"
    data = bytearray(1024)
    data[:6] = b"\x7fELF\x02\x01"
    struct.pack_into("<H", data, 18, machine)
    struct.pack_into("<QQ", data, 32, 64, 512)
    struct.pack_into("<HHHH", data, 54, 56, 1, 64, 5)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0, 0, 0, 1024, 1024, alignment)
    data[128:128 + len(strings)] = strings
    struct.pack_into("<qQqQ", data, 224, 1, 1, 0, 0)
    struct.pack_into("<HHIII", data, 272, 1, 1, 1, 16, 0)
    struct.pack_into("<IHHII", data, 288, 0, 0, 1, len(provider), 0)
    def section(index, kind, offset, size, link=0):
        struct.pack_into("<IIQQQQIIQQ", data, 512 + index * 64, 0, kind, 0, 0, offset, size, link, 0, 0, 0)
    section(1, 3, 128, len(strings))
    section(2, 6, 224, 32, 1)
    section(3, 0x6ffffffe, 272, 32, 1)
    section(4, 0x6ffffffd, 320, 32, 1)  # Definitions must not inflate required ABI.
    return bytes(data)


def pe_fixture(imports=("KERNEL32.dll",), delayed=("icuuc76.dll",)):
    data = bytearray(2048)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, 128)
    data[128:132] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 132, 0x8664, 1, 0, 0, 0, 240, 0)
    struct.pack_into("<H", data, 152, 0x20b)
    struct.pack_into("<I", data, 152 + 60, 512)
    struct.pack_into("<I", data, 152 + 108, 16)
    struct.pack_into("<8sIIIIIIHHI", data, 392, b".rdata\0\0", 1536, 0x1000, 1536, 512, 0, 0, 0, 0, 0)
    string = 1200
    for directory, start, stride, names in ((1, 512, 20, imports), (13, 768, 32, delayed)):
        struct.pack_into("<II", data, 152 + 112 + directory * 8, 0x1000 + start - 512, stride * (len(names) + 1))
        for index, name in enumerate(names):
            encoded = name.encode() + b"\0"
            data[string:string + len(encoded)] = encoded
            values = [0] * (stride // 4)
            values[3 if directory == 1 else 1] = 0x1000 + string - 512
            if directory == 13:
                values[0] = 1
            struct.pack_into("<" + "I" * len(values), data, start + index * stride, *values)
            string += len(encoded)
    return bytes(data)


BUILD = {"schema": 1, "version": "0.1.0-prealpha", "maturity": "pre-alpha", "channel": "Experimental",
         "build_id": "fixture", "source_revision": "1" * 40, "source_tree": "2" * 40,
         "source_dirty": False, "source_fingerprint": "3" * 64, "source_origin": "git",
         "source_untracked_count": 0, "source_untracked_fingerprint": hashlib.sha256(b"").hexdigest(),
         "platform": "Android", "architecture": "arm64-v8a", "configuration": "Release",
         "compiler": "Clang", "compiler_version": "fixture"}


class DistributionChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="judas-distribution-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_elf_actual_requirements_ignore_exported_version_decoy(self):
        result = binary_dependencies.elf(elf_fixture())
        self.assertEqual(result["needed"], ["libc.so.6"])
        self.assertEqual(result["version_requirements"], {"libc.so.6": ["GLIBC_2.39"]})
        self.assertEqual(result["load_alignments"], [16384])

    def test_elf_malformed_tables_and_chains_fail(self):
        for change in (lambda data: struct.pack_into("<Q", data, 40, 999999),
                       lambda data: struct.pack_into("<I", data, 272 + 12, 1),
                       lambda data: struct.pack_into("<I", data, 272 + 8, 99999)):
            data = bytearray(elf_fixture()); change(data)
            with self.assertRaises(ValueError):
                binary_dependencies.elf(bytes(data))
        with self.assertRaises(ValueError):
            binary_dependencies.elf(b"\x7fELF\x01\x01")

    def test_sdk_declared_glibc_gate_rejects_newer_binary(self):
        (self.root / "judas").write_bytes(elf_fixture())
        manifest = {"tools": ["judas"], "build": dict(BUILD, platform="Linux", architecture="x86_64")}
        with patch.object(inspect_sdk.package_sdk, "verify", return_value=manifest), patch.object(inspect_sdk.package_sdk, "digest", return_value="fixture"):
            self.assertTrue(inspect_sdk.inspect_sdk(self.root, max_glibc="2.39")["passed"])
            (self.root / "judas").write_bytes(elf_fixture(glibc="2.43"))
            result = inspect_sdk.inspect_sdk(self.root, max_glibc="2.39")
            self.assertFalse(result["passed"])
            self.assertEqual(result["highest_required_abi"]["GLIBC"], "2.43")

    def test_pe_regular_and_delayed_imports(self):
        result = binary_dependencies.pe(pe_fixture())
        self.assertEqual(result["needed"], ["icuuc76.dll", "kernel32.dll"])
        self.assertEqual(result["machine"], 0x8664)
        data = bytearray(pe_fixture()); struct.pack_into("<I", data, 512 + 12, 999999)
        with self.assertRaises(ValueError):
            binary_dependencies.pe(bytes(data))

    def test_windows_transitive_missing_import_detected(self):
        (self.root / "judas.exe").write_bytes(pe_fixture())
        (self.root / "icuuc76.dll").write_bytes(pe_fixture(imports=("KERNEL32.dll",), delayed=()))
        system = self.root / "system"; system.mkdir(); (system / "kernel32.dll").write_bytes(b"Windows fixture")
        manifest = {"tools": ["judas.exe"], "build": dict(BUILD, platform="Windows", architecture="AMD64")}
        with patch.object(inspect_sdk.package_sdk, "verify", return_value=manifest), patch.object(inspect_sdk.package_sdk, "digest", return_value="fixture"):
            self.assertTrue(inspect_sdk.inspect_sdk(self.root, system_directory=system)["passed"])
            (self.root / "icuuc76.dll").unlink()
            result = inspect_sdk.inspect_sdk(self.root, system_directory=system)
            self.assertFalse(result["passed"])
            self.assertIn("icuuc76.dll", "\n".join(result["errors"]))

    def apk(self, mutate=None, alignment=16384, dependency='libc.so'):
        files = {"assets/content/project.json": b'{"game":"fixture"}'}
        libraries = {f"lib/arm64-v8a/{name}": elf_fixture(machine=183, alignment=alignment, needed=dependency, required_label='LIBC') for name in ("libmain.so", "libSDL2.so", "libc++_shared.so")}
        def records(data, prefix=""):
            return [{"path": name.removeprefix(prefix), "bytes": len(value), "sha256": hashlib.sha256(value).hexdigest()} for name, value in sorted(data.items())]
        metadata = {"assets/judas-build-info.json": {"schema": 1, "kind": "judas-android", "abis": {"arm64-v8a": BUILD}},
                    "assets/judas-content.json": {"schema": 1, "files": records(files, "assets/content/")},
                    "assets/judas-native.json": {"schema": 1, "files": records(libraries)}}
        files.update(libraries); files.update({name: json.dumps(value).encode() for name, value in metadata.items()})
        if mutate:
            mutate(files)
        apk = self.root / "game.apk"
        with zipfile.ZipFile(apk, "w", compression=zipfile.ZIP_STORED) as archive:
            for name, payload in files.items():
                archive.writestr(name, payload)
        return apk

    def test_apk_verified_native_and_content_bytes(self):
        result = verify_android.verify_apk(self.apk(), expected_identities={"arm64-v8a": BUILD})
        self.assertEqual(result["contentFiles"], 1)
        self.assertEqual(len(result["shippedNative"]), 3)

    def test_apk_corruption_missing_and_phantom_content_rejected(self):
        for mutate in (lambda files: files.__setitem__("assets/content/project.json", b"corrupt"),
                       lambda files: files.pop("lib/arm64-v8a/libSDL2.so"),
                       lambda files: files.__setitem__("assets/content/untracked.json", b"unknown")):
            with self.assertRaises((ValueError, KeyError)):
                verify_android.verify_apk(self.apk(mutate))

    def test_apk_bad_alignment_identity_and_budget_rejected(self):
        with self.assertRaisesRegex(ValueError, "alignment"):
            verify_android.verify_apk(self.apk(alignment=4096))
        with self.assertRaisesRegex(ValueError, "identity disagrees"):
            verify_android.verify_apk(self.apk(), expected_identities={"arm64-v8a": dict(BUILD, channel="main")})
        with self.assertRaisesRegex(ValueError, "budget"):
            verify_android.verify_apk(self.apk(), max_unpacked=10)

    def test_apk_canonical_android_abi_name_and_wrong_cpu_identity(self):
        self.assertTrue(verify_android.verify_apk(self.apk())["passed"])
        def wrong_cpu(files):
            name = "assets/judas-build-info.json"; metadata = json.loads(files[name])
            metadata["abis"]["arm64-v8a"]["architecture"] = "x86_64"
            files[name] = json.dumps(metadata).encode()
        with self.assertRaisesRegex(ValueError, "actual ABI architecture"):
            verify_android.verify_apk(self.apk(wrong_cpu))

    def test_apk_external_receipt_must_match_shipped_not_build_input(self):
        path = self.apk(); result = verify_android.verify_apk(path)
        receipt = {"sha256": result["sha256"], "engine": result["engine"], "shippedNative": result["shippedNative"]}
        self.assertTrue(verify_android.verify_apk(path, receipt=receipt)["passed"])
        receipt["shippedNative"] = []
        with self.assertRaisesRegex(ValueError, "external receipt"):
            verify_android.verify_apk(path, receipt=receipt)

    def test_apk_unshipped_or_desktop_native_dependency_rejected(self):
        for dependency in ('libmissing.so', 'libc.so.6'):
            with self.assertRaisesRegex(ValueError, 'native imports'):
                verify_android.verify_apk(self.apk(dependency=dependency))

    def test_android_publication_restores_entire_previous_bundle_on_failure(self):
        source = self.root / "signed.apk"; source.write_bytes(b"new signed APK")
        output = self.root / "published.apk"
        targets = (output, output.with_suffix(".build.json"), output.with_suffix(".sha256"))
        for path in targets:
            path.write_bytes(b"old " + path.name.encode())
        original = {path: path.read_bytes() for path in targets}; replace = os.replace; calls = []
        def fail_once(source, destination):
            calls.append(str(destination))
            if len(calls) == 4:
                raise OSError("injected receipt publication failure")
            return replace(source, destination)
        with patch.object(build_android.os, "replace", side_effect=fail_once), self.assertRaisesRegex(OSError, "injected"):
            build_android.publish_apk(source, output, {"engine": {}})
        self.assertEqual({path: path.read_bytes() for path in targets}, original)
        build_android.publish_apk(source, output, {"engine": {}})
        self.assertEqual(output.read_bytes(), source.read_bytes())
        receipt = json.loads(targets[1].read_text())
        self.assertEqual(receipt["sha256"], hashlib.sha256(source.read_bytes()).hexdigest())
        self.assertIn(receipt["sha256"], targets[2].read_text())

    def test_jdk_requires_complete_coherent_modern_tools(self):
        home = self.root / "jdk"; binary = home / "bin"; binary.mkdir(parents=True)
        for name in ("javac", "java", "keytool"):
            (binary / name).write_text("fixture")
        command = type("Result", (), {"returncode": 0, "stdout": "javac 17.0.20\n", "stderr": ""})()
        with patch.object(build_android.subprocess, "run", return_value=command):
            tools, env, label = build_android.jdk_tools({"JAVA_HOME": str(home), "PATH": "/fixture"})
            self.assertEqual(tools["java"].parent, tools["javac"].parent)
            self.assertEqual(Path(env["JAVA_HOME"]), home.resolve())
            command.stdout = "javac 11.0.1"
            with self.assertRaisesRegex(ValueError, "17"):
                build_android.jdk_tools({"JAVA_HOME": str(home)})
        (binary / "keytool").unlink()
        with self.assertRaisesRegex(ValueError, "Incomplete JDK"):
            build_android.jdk_tools({"JAVA_HOME": str(home)})

    def test_android_stale_configured_identity_fails_before_native_compile(self):
        sdk = self.root / "sdk"
        paths = [f"build-tools/{build_android.BUILD_TOOLS}/{name}" for name in ("aapt2", "d8", "apksigner", "zipalign")]
        paths += ["platforms/android-36/android.jar", f"ndk/{build_android.NDK}/build/cmake/android.toolchain.cmake"]
        for name in paths:
            path = sdk / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b"tool fixture")
        arguments = ["game.judasproj", "--output", str(self.root / "game.apk"), "--sdk", str(sdk),
                     "--cache", str(self.root / "cache"), "--abis", "arm64-v8a"]
        args, parser = build_android.parse_args(arguments)
        commands = []
        def configure(command):
            commands.append(command)
            directory = args.cache / "android-arm64-v8a"; directory.mkdir(parents=True)
            (directory / "judas-build-info-Release.json").write_text(json.dumps(dict(BUILD, source_fingerprint="4" * 64)))
        with patch.object(build_android, "parse_args", return_value=(args, parser)), \
             patch.object(build_android, "jdk_tools", return_value=({}, dict(os.environ, JAVA_HOME="/fixture", PATH=os.environ.get("PATH", "")), "fixture")), \
             patch.object(build_android, "probe_identity", return_value=BUILD), \
             patch.object(build_android, "run", side_effect=configure), \
             patch.object(build_android.sys, "platform", "linux"), \
             patch.object(build_android, "fcntl") as lock:
            # The Windows fixture also exercises this ordering control without
            # needing Linux's locking module or executing any compiler.
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                build_android.main()
        self.assertEqual(len(commands), 1)
        self.assertNotIn("--build", commands[0])

    def builder(self, change_during_build=False):
        sdk = self.root / 'sdk'; cache = self.root / 'cache'; source = self.root / 'source'
        paths = [f'build-tools/{build_android.BUILD_TOOLS}/{name}' for name in ('aapt2', 'd8', 'apksigner', 'zipalign')]
        paths += ['platforms/android-36/android.jar', f'ndk/{build_android.NDK}/build/cmake/android.toolchain.cmake']
        for name in paths:
            path = sdk / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'tool fixture')
        for name in ('assets/branding/judas-icon.png', 'third_party/android/SDL-LICENSE.txt', 'third_party/android/GLM-COPYING.txt'):
            path = source / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'asset fixture')
        output = self.root / 'game.apk'
        args, parser = build_android.parse_args(['game.judasproj', '--output', str(output), '--sdk', str(sdk), '--cache', str(cache), '--abis', 'arm64-v8a', '--exporter', str(self.root / 'exporter')])
        tools = {name: self.root / 'jdk/bin' / name for name in ('java', 'javac', 'keytool')}
        commands = []
        def run(command, **kwargs):
            command = list(map(str, command)); commands.append(command)
            def following(flag):
                return Path(command[command.index(flag) + 1])
            if command[:2] == ['cmake', '-S']:
                build = following('-B'); build.mkdir(parents=True, exist_ok=True)
                (build / 'judas-build-info-Release.json').write_text(json.dumps(BUILD))
                for relative in ('libmain.so', '_deps/judas_sdl-build/libSDL2.so'):
                    path = build / relative; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(elf_fixture(machine=183, needed='libc.so', required_label='LIBC'))
                java = build / '_deps/judas_sdl-src/android-project/app/src/main/java/org/libsdl/app/HIDDeviceManager.java'
                java.parent.mkdir(parents=True, exist_ok=True)
                java.write_text('mContext.registerReceiver(mUsbBroadcast, filter);\nmContext.registerReceiver(mBluetoothBroadcast, filter);')
                library = sdk / f'ndk/{build_android.NDK}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so'
                library.parent.mkdir(parents=True, exist_ok=True); library.write_bytes(elf_fixture(machine=183, needed='libc.so', required_label='LIBC'))
                stub = library.parent / '26/libc.so'; stub.parent.mkdir(parents=True); stub.write_bytes(b'NDK public stub fixture')
            elif command[:2] == ['cmake', '--build']:
                if change_during_build:
                    (Path(command[2]) / 'judas-build-info-Release.json').write_text(json.dumps(dict(BUILD, source_fingerprint='4' * 64)))
            elif command[0] == str(args.exporter.resolve()):
                package = Path(command[2]); package.mkdir(parents=True); (package / 'project.json').write_bytes(b'project content')
            elif Path(command[0]).name == 'aapt2' and command[1] == 'link':
                with zipfile.ZipFile(following('-o'), 'w') as archive:
                    for path in following('-A').rglob('*'):
                        if path.is_file():
                            archive.write(path, 'assets/' + path.relative_to(following('-A')).as_posix())
            elif command[0] == str(tools['javac']):
                (following('-d') / 'Main.class').write_bytes(b'compiled class fixture')
            elif Path(command[0]).name == 'd8':
                (following('--output') / 'classes.dex').write_bytes(b'dex fixture')
            elif Path(command[0]).name == 'zipalign' and '-f' in command:
                import shutil
                shutil.copyfile(command[-2], command[-1])
            elif command[0] == str(tools['keytool']):
                following('-keystore').write_bytes(b'fixture signing key, not real')
            elif Path(command[0]).name == 'apksigner' and command[1] == 'sign':
                import shutil
                shutil.copyfile(command[-1], following('--out'))
        with patch.object(build_android, 'ROOT', source), patch.object(build_android, 'parse_args', return_value=(args, parser)), \
             patch.object(build_android, 'jdk_tools', return_value=(tools, dict(os.environ, JAVA_HOME=str(self.root / 'jdk')), 'javac fixture')), \
             patch.object(build_android, 'probe_identity', return_value=BUILD), patch.object(build_android, 'run', side_effect=run), \
             patch.object(build_android.sys, 'platform', 'linux'), patch.object(build_android, 'fcntl'), patch.dict(os.environ), \
             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            if change_during_build:
                with self.assertRaises(SystemExit):
                    build_android.main()
            else:
                build_android.main()
        return output, commands

    def test_mocked_complete_builder_reaches_compiler_signing_and_verified_publication(self):
        output, commands = self.builder()
        receipt = json.loads(output.with_suffix('.build.json').read_text())
        self.assertTrue(verify_android.verify_apk(output, receipt=receipt)['passed'])
        names = [Path(command[0]).name for command in commands]
        self.assertIn('javac', names); self.assertIn('keytool', names); self.assertIn('apksigner', names)
        # All external tools are simulated; only packaging/ZIP/verification/I/O
        # are real here. This is not a signature, compiler or Android run pass.

    def test_native_build_reconfiguration_cannot_publish_stale_identity(self):
        output, commands = self.builder(change_during_build=True)
        self.assertFalse(output.exists())
        self.assertTrue(any('--build' in command for command in commands))
        self.assertFalse(any(Path(command[0]).name == 'apksigner' for command in commands))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(DistributionChecks))
    report = {"schema": 1, "suite": "prealpha-distribution", "tests": result.testsRun,
              "failures": len(result.failures), "errors": len(result.errors), "passed": result.wasSuccessful(),
              "scope": "Independent binary/APK fixtures and I/O failure injection; no native runtime/hardware claim"}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n")
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
