#!/usr/bin/env python3
"""Exercise packaged Python entrypoints without implicit SDK cache mutations.

Uses fresh, writable, checksum-inventoried SDK fixtures and tiny synthetic ELF/APK
metadata, not engine builds. Each tool runs as a separate ordinary Python process
without -B or inherited PYTHON* settings. Outputs stay outside the SDK.
"""

import argparse
import datetime
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import zipfile


ROOT = Path(__file__).resolve().parents[1]
BARRIER = (b"# Packaged tools must not add Python caches to the checksum-verified SDK.\n"
           b"sys.dont_write_bytecode = True\n\n")
PYTHON_TOOLS = ("inspect_sdk.py", "verify_android.py", "package_sdk.py", "binary_dependencies.py")
BUILD = {
    "schema": 1, "version": "0.0.0-fixture", "maturity": "test-fixture",
    "channel": "Local", "build_id": "sdk-readonly-fixture", "source_revision": "unknown",
    "source_tree": "unknown", "source_dirty": True, "source_fingerprint": "unknown",
    "source_origin": "unknown", "source_untracked_count": 0,
    "source_untracked_fingerprint": "0" * 64, "platform": "Linux",
    "architecture": "x86_64", "configuration": "fixture",
    "compiler": "synthetic-metadata", "compiler_version": "not-a-native-build",
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def snapshot(directory):
    return {path.relative_to(directory).as_posix(): {"bytes": path.stat().st_size,
            "sha256": sha(path.read_bytes())} for path in sorted(directory.rglob("*"))
            if path.is_file()}


def elf(machine):
    """Minimal metadata-only ELF64: one 16KiB-aligned load, one null section."""
    data = bytearray(256)
    data[:6] = b"\x7fELF\x02\x01"
    struct.pack_into("<H", data, 18, machine)
    struct.pack_into("<QQ", data, 32, 64, 128)
    struct.pack_into("<HHHH", data, 54, 56, 1, 64, 1)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0, 0, 0, 256, 256, 16384)
    return bytes(data)


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, sort_keys=True, indent=2) + "\n", encoding="utf-8")


def make_sdk(directory, source, *, remove_barrier):
    directory.mkdir(parents=True)
    (directory / "tools").mkdir()
    copied = {}
    for name in PYTHON_TOOLS:
        payload = (source / name).read_bytes()
        if name in ("inspect_sdk.py", "verify_android.py"):
            if payload.count(BARRIER) != 1:
                raise ValueError("expected one early cache-write barrier in " + name)
            if remove_barrier:
                payload = payload.replace(BARRIER, b"", 1)
        (directory / "tools" / name).write_bytes(payload)
        copied[name] = sha(payload)
    (directory / "judas").write_bytes(elf(62))
    write_json(directory / "BUILD_INFO.json", BUILD)
    write_json(directory / "packaging/sdk-inventory.json", {
        "schema": 1, "tools": ["judas"], "windows_tools": [],
        "files": ["tools/" + name for name in PYTHON_TOOLS],
        "directories": [], "current_markdown": "docs", "excluded_markdown": [],
    })
    records = [{"path": path, **record} for path, record in snapshot(directory).items()]
    write_json(directory / "SDK_MANIFEST.json", {
        "schema": 1, "kind": "judas-native-sdk", "preset": "development",
        "build": BUILD, "tools": ["judas"], "files": records,
    })
    sums = "".join(f"{record['sha256']}  {record['path']}\n" for record in records)
    sums += sha((directory / "SDK_MANIFEST.json").read_bytes()) + "  SDK_MANIFEST.json\n"
    (directory / "SHA256SUMS").write_text(sums, encoding="utf-8", newline="\n")
    return copied


def make_apk(path):
    content = {"assets/content/project.json": b'{"purpose":"metadata-only regression fixture"}'}
    native = {"lib/arm64-v8a/" + name: elf(183)
              for name in ("libmain.so", "libSDL2.so", "libc++_shared.so")}
    records = lambda entries, prefix="": [{"path": name.removeprefix(prefix),
        "bytes": len(data), "sha256": sha(data)} for name, data in sorted(entries.items())]
    android = dict(BUILD, platform="Android", architecture="arm64-v8a")
    metadata = {
        "assets/judas-build-info.json": {"schema": 1, "kind": "judas-android",
                                          "abis": {"arm64-v8a": android}},
        "assets/judas-content.json": {"schema": 1, "files": records(content, "assets/content/")},
        "assets/judas-native.json": {"schema": 1, "files": records(native)},
    }
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as archive:
        for name, data in sorted({**content, **native}.items()):
            archive.writestr(name, data)
        for name, data in sorted(metadata.items()):
            archive.writestr(name, json.dumps(data))


def run(output, name, command, expected):
    # -E ignores PYTHONDONTWRITEBYTECODE/PYTHONPYCACHEPREFIX. There is no -B;
    # cache safety must come from the actual distributed entrypoint.
    argv = [sys.executable, "-E", *map(str, command)]
    result = subprocess.run(argv, cwd=output, capture_output=True, text=True, timeout=15)
    (output / (name + ".stdout.log")).write_text(result.stdout, encoding="utf-8")
    (output / (name + ".stderr.log")).write_text(result.stderr, encoding="utf-8")
    record = {"name": name, "argv": argv, "cwd": str(output),
              "exit": result.returncode, "expected_exit": expected,
              "stdout": result.stdout, "stderr": result.stderr}
    if result.returncode != expected:
        raise AssertionError(json.dumps(record, indent=2))
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", type=Path, default=ROOT / "tools")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    result = {"schema": 1, "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "scope": "isolated packaged Python invocation; synthetic metadata only, no engine/platform qualification",
              "commands": [], "cases": []}
    try:
        apk = output / "fixture.apk"
        make_apk(apk)
        for removed in (True, False):
            for tool in ("inspect_sdk.py", "verify_android.py"):
                name = ("original" if removed else "corrected") + "-" + Path(tool).stem
                sdk = output / name
                sources = make_sdk(sdk, args.tools.resolve(), remove_barrier=removed)
                initial = snapshot(sdk)
                verify = [sdk / "tools/package_sdk.py", "verify", sdk]
                result["commands"].append(run(output, name + "-before", verify, 0))
                command = [sdk / "tools" / tool]
                command += ([sdk, "--output", output / (name + ".json")]
                            if tool == "inspect_sdk.py" else [apk])
                result["commands"].append(run(output, name + "-tool", command,
                                              1 if removed and tool == "inspect_sdk.py" else 0))
                after = snapshot(sdk)
                changed = sorted(key for key in set(initial) | set(after)
                                 if initial.get(key) != after.get(key))
                result["commands"].append(run(output, name + "-after", verify, 1 if removed else 0))
                if removed:
                    if len(changed) != 2 or not all("/__pycache__/" in key and key.endswith(".pyc") for key in changed):
                        raise AssertionError("original entrypoint did not expose exactly two generated caches")
                elif changed:
                    raise AssertionError("corrected entrypoint changed packaged SDK: " + repr(changed))
                case = {"name": name, "source_sha256": sources, "changed_files": changed,
                        "before": initial, "after": after, "expected_mutation": removed}
                result["cases"].append(case)
                if not removed:
                    # Do not fix self-mutation by ignoring unexpected caches.
                    unexpected = sdk / "tools/__pycache__/unexpected.pyc"
                    unexpected.parent.mkdir(exist_ok=True)
                    unexpected.write_bytes(b"deliberately unmanifested regression data")
                    result["commands"].append(run(output, name + "-unexpected-cache", verify, 1))
                    unexpected.unlink()
                    # Existing content hashes must remain enforced as well.
                    (sdk / "judas").write_bytes(b"deliberately corrupted binary metadata")
                    result["commands"].append(run(output, name + "-changed-payload", verify, 1))
        result["passed"] = True
    finally:
        write_json(output / "RESULTS.json", result)
    print(json.dumps({"passed": result.get("passed", False), "cases": len(result["cases"]),
                      "commands": len(result["commands"]), "output": str(output)}))


if __name__ == "__main__":
    main()
