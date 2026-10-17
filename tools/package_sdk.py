#!/usr/bin/env python3
"""Stage/verify a native Judas SDK without Git, network access or third-party Python packages.

The inventory is shared with CMake. Build artifacts, current developer docs and
required native DLLs are copied; projects and historical research are not SDK payloads.
Manifests detect accidental omissions/corruption, not malicious replacement.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = "SDK_MANIFEST.json"
CHECKSUMS = "SHA256SUMS"
IDENTITY_FIELDS = (
    "schema", "version", "maturity", "channel", "build_id", "source_revision",
    "source_tree", "source_dirty", "source_fingerprint", "source_origin",
    "source_untracked_count", "source_untracked_fingerprint",
    "platform", "architecture", "configuration", "compiler", "compiler_version",
)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read_json(path):
    with path.open(encoding="utf-8") as stream:
        return json.load(stream)


def identity(path, release=False):
    return validate_identity(read_json(path), release=release, origin=str(path))


def validate_identity(value, release=False, origin="build identity"):
    missing = set(IDENTITY_FIELDS) - value.keys()
    if missing or value.get("schema") != 1:
        raise ValueError(f"Invalid build identity {origin}; missing {sorted(missing)}")
    if any(not isinstance(value[field], str) for field in IDENTITY_FIELDS if field not in ("schema", "source_dirty", "source_untracked_count")):
        raise ValueError("Build identity fields must use their declared string/Boolean types")
    if value["channel"] not in ("main", "Experimental", "Local"):
        raise ValueError("Unknown build channel")
    if not isinstance(value["source_dirty"], bool):
        raise ValueError("Build identity source_dirty must be Boolean")
    if type(value["source_untracked_count"]) is not int or value["source_untracked_count"] < 0:
        raise ValueError("Build identity source_untracked_count must be a nonnegative integer")
    if not re.fullmatch(r"[0-9a-f]{64}", value["source_untracked_fingerprint"]):
        raise ValueError("Build identity source_untracked_fingerprint must be a SHA-256")
    if release and (value["source_origin"] not in ("git", "archive") or value["source_dirty"] or
                    value["source_untracked_count"] != 0 or
                    value["configuration"] != "Release" or value["channel"] == "Local" or
                    not re.fullmatch(r"[0-9a-f]{40}", value["source_revision"]) or
                    not re.fullmatch(r"[0-9a-f]{40}", value["source_tree"]) or
                    not re.fullmatch(r"[0-9a-f]{64}", value["source_fingerprint"])):
        raise ValueError("Release SDK requires clean Git/archive provenance with no untracked build inputs, Release configuration and main/Experimental channel; checkpoint inputs or build a verified source archive")
    return value


def probe_identity(executable, timeout=10, limit=16384):
    """Read a native identity with bounded time and bounded captured memory on Windows/Linux."""
    child = subprocess.Popen([str(executable), "--build-info-json"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    done = threading.Event()
    captured = bytearray()
    started = time.monotonic()

    def read():
        try:
            while True:
                block = child.stdout.read(min(4096, limit + 1 - len(captured)))
                if not block:
                    break
                captured.extend(block)
                if len(captured) > limit:
                    break
        finally:
            done.set()

    reader = threading.Thread(target=read, daemon=True)
    reader.start()
    try:
        if not done.wait(timeout):
            raise ValueError(f"Tool identity probe timed out: {executable}")
        if len(captured) > limit:
            raise ValueError(f"Tool identity output exceeds {limit} bytes: {executable}")
        if child.wait(timeout=max(.01, timeout - (time.monotonic() - started))) != 0:
            raise ValueError(f"Tool identity probe failed: {executable}")
        return json.loads(captured.decode("utf-8"))
    finally:
        if child.poll() is None:
            child.kill()
            child.wait()
        reader.join(timeout=1)
        child.stdout.close()


def relative_path(value):
    path = Path(value)
    if not value or path.is_absolute() or ".." in path.parts or "\\" in value or ":" in value:
        raise ValueError(f"Unsafe inventory/manifest path: {value!r}")
    return path


def inventory(path):
    value = read_json(path)
    if value.get("schema") != 1:
        raise ValueError("Unsupported SDK inventory schema")
    tools = value["tools"] + value["windows_tools"]
    if len(tools) != len(set(tools)) or any(not t.startswith("judas") or not t.replace("_", "").isalnum() for t in tools):
        raise ValueError("Invalid or duplicate SDK tool name")
    for name in value["files"] + value["directories"] + [value["current_markdown"]] + value.get("excluded_markdown", []):
        relative_path(name)
    return value


def file_records(directory):
    records = []
    for path in sorted(directory.rglob("*"), key=lambda value: value.relative_to(directory).as_posix()):
        if path.is_symlink():
            raise ValueError(f"SDK symlinks are not supported: {path}")
        if path.is_file() and path.relative_to(directory).as_posix() not in (MANIFEST, CHECKSUMS):
            records.append({"path": path.relative_to(directory).as_posix(),
                            "bytes": path.stat().st_size, "sha256": digest(path)})
    return records


def verify(directory):
    manifest = read_json(directory / MANIFEST)
    if manifest.get("schema") != 1 or manifest.get("kind") != "judas-native-sdk":
        raise ValueError("Unsupported SDK manifest")
    build = identity(directory / "BUILD_INFO.json", release=manifest.get("preset") == "release")
    if manifest["build"] != build:
        raise ValueError("SDK build identity does not match manifest")
    expected = manifest["files"]
    if len({entry["path"] for entry in expected}) != len(expected):
        raise ValueError("Duplicate SDK manifest file")
    for entry in expected:
        relative_path(entry["path"])
    if file_records(directory) != expected:
        raise ValueError("SDK file inventory/checksum mismatch (missing, changed or unexpected file)")
    names = {entry["path"] for entry in expected}
    spec = inventory(directory / "packaging/sdk-inventory.json")
    windows = build["platform"].lower() == "windows"
    tools = [name + (".exe" if windows else "") for name in
             spec["tools"] + (spec["windows_tools"] if windows else [])]
    if manifest["tools"] != tools or not set(tools) <= names:
        raise ValueError("SDK manifest omits a required tool")
    checksums = "".join(f"{entry['sha256']}  {entry['path']}\n" for entry in expected)
    checksums += f"{digest(directory / MANIFEST)}  {MANIFEST}\n"
    if (directory / CHECKSUMS).read_text(encoding="utf-8") != checksums:
        raise ValueError("SDK SHA256SUMS mismatch")
    return manifest


def stage(args):
    source, binaries, destination = args.source.resolve(), args.bin_dir.resolve(), args.output.resolve()
    if destination in (source, binaries, Path(destination.anchor)) or destination in source.parents or destination in binaries.parents:
        raise ValueError("SDK destination must be a separate generated directory")
    if destination.exists() and not args.replace:
        raise ValueError("Destination exists; use a new directory or explicit --replace for a generated SDK")
    if destination.exists() and any(destination.iterdir()) and not (destination / MANIFEST).is_file():
        raise ValueError("Existing destination has no SDK manifest; choose a new generated directory instead of replacing unrelated files")
    spec = inventory(args.inventory)
    release = getattr(args, "release", False)
    build = identity(args.build_info, release=release)
    suffix = ".exe" if build["platform"].lower() == "windows" else ""
    tools = spec["tools"] + (spec["windows_tools"] if suffix else [])
    for name in tools:
        if not (binaries / (name + suffix)).is_file():
            raise ValueError(f"Required SDK tool is missing: {binaries / (name + suffix)}")
    # Preparation failure must not remove a previously published SDK.
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=destination.name + ".prepare-", dir=destination.parent) as temporary:
        prepared = Path(temporary) / "payload"
        prepared.mkdir()

        def copy(origin, relative):
            target = prepared / relative_path(relative)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(origin, target)

        for name in tools:
            filename = name + suffix
            tool = binaries / filename
            if args.probe_tools and name not in spec["windows_tools"]:
                if probe_identity(tool) != build:
                    raise ValueError(f"Tool build identity mismatch: {name}")
            copy(tool, filename)
        for name in spec["files"]:
            copy(source / name, name)
        for name in spec["directories"]:
            shutil.copytree(source / name, prepared / name)
        for path in sorted((source / spec["current_markdown"]).glob("*.md")):
            name = path.relative_to(source).as_posix()
            if name not in spec.get("excluded_markdown", []):
                copy(path, name)
        copy(source / "tools/package_sdk.py", "tools/package_sdk.py")
        copy(args.inventory, "packaging/sdk-inventory.json")
        # Existing Windows exporter expects this layout alongside each tool.
        if suffix:
            shutil.copytree(binaries / "windows-licenses", prepared / "windows-licenses")
            copy(binaries / "windows-runtime/required-dlls.txt", "windows-runtime/required-dlls.txt")
            dlls = (prepared / "windows-runtime/required-dlls.txt").read_text().splitlines()
            if not dlls or len(dlls) != len(set(dlls)):
                raise ValueError("Windows runtime DLL inventory is empty or duplicated")
            for line in dlls:
                relative_path(line)
                if Path(line).name != line or not line.lower().endswith(".dll"):
                    raise ValueError("Invalid required DLL name")
                copy(binaries / line, line)
                copy(binaries / line, "windows-runtime/" + line)
        (prepared / "BUILD_INFO.json").write_text(json.dumps(build, sort_keys=True, indent=2) + "\n", encoding="utf-8", newline="\n")
        manifest = {"schema": 1, "kind": "judas-native-sdk", "preset": "release" if release else "development", "build": build,
                    "tools": [name + suffix for name in tools], "files": file_records(prepared)}
        (prepared / MANIFEST).write_text(json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8", newline="\n")
        sums = "".join(f"{entry['sha256']}  {entry['path']}\n" for entry in manifest["files"])
        sums += f"{digest(prepared / MANIFEST)}  {MANIFEST}\n"
        (prepared / CHECKSUMS).write_text(sums, encoding="utf-8", newline="\n")
        verify(prepared)
        # Keep the prior directory until replacement succeeds; no half SDK is published.
        previous = destination.with_name(destination.name + ".previous-" + str(os.getpid()))
        if previous.exists():
            raise ValueError(f"Previous staging directory already exists: {previous}")
        if destination.exists():
            destination.rename(previous)
        try:
            prepared.rename(destination)
        except BaseException:
            if previous.exists():
                previous.rename(destination)
            raise
        if previous.exists():
            shutil.rmtree(previous)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    package = sub.add_parser("stage")
    package.add_argument("--source", type=Path, default=ROOT)
    package.add_argument("--inventory", type=Path, default=ROOT / "packaging/sdk-inventory.json")
    package.add_argument("--bin-dir", type=Path, required=True)
    package.add_argument("--build-info", type=Path, required=True)
    package.add_argument("--output", type=Path, required=True)
    package.add_argument("--replace", action="store_true")
    package.add_argument("--probe-tools", action="store_true", help="Execute each public tool's bounded identity query before publication")
    package.add_argument("--release", action="store_true", help="Require clean known source provenance and Release configuration before publication")
    check = sub.add_parser("verify")
    check.add_argument("directory", type=Path)
    args = parser.parse_args()
    try:
        result = stage(args) if args.command == "stage" else verify(args.directory)
        print(json.dumps({"ok": True, "build": result["build"], "files": len(result["files"]), "tools": len(result["tools"])}, sort_keys=True))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, f"SDK packaging failed: {error}\n")


if __name__ == "__main__":
    main()
