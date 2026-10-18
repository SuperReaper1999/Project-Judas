#!/usr/bin/env python3
"""Qualify a verified native SDK's actual ABI/dependency requirements.

Metadata inspection is non-executing. --host-check additionally runs ldd for a
trusted Linux SDK. A passing receipt applies only to the named host/baseline;
it does not certify GPU drivers, other distributions or Windows API availability.
"""

import argparse
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys

# Packaged tools must not add Python caches to the checksum-verified SDK.
sys.dont_write_bytecode = True

import binary_dependencies
import package_sdk


def version(value):
    if not re.fullmatch(r"\d+(?:\.\d+)+", value):
        raise ValueError(f"Invalid numeric ABI version: {value}")
    return tuple(map(int, value.split(".")))


def host_identity():
    result = {"system": platform.system(), "machine": platform.machine()}
    if sys.platform.startswith("linux"):
        result["os_release"] = platform.freedesktop_os_release()
    return result


def inspect_sdk(directory, *, host_check=False, max_glibc=None, require_os=None, system_directory=None):
    manifest = package_sdk.verify(directory)
    windows = manifest["build"]["platform"].lower() == "windows"
    names = manifest["tools"] + ([p.name for p in sorted(directory.glob("*.dll"))] if windows else [])
    details, errors, maxima = {}, [], {}
    expected_machine = {"x86_64": 0x8664 if windows else 62, "amd64": 0x8664 if windows else 62,
                        "aarch64": 0xaa64 if windows else 183, "arm64": 0xaa64 if windows else 183}.get(manifest["build"]["architecture"].lower())
    for name in names:
        try:
            info = binary_dependencies.inspect((directory / name).read_bytes())
        except ValueError as error:
            errors.append(f"{name}: {error}")
            continue
        details[name] = info
        if (windows and not info["format"].startswith("PE")) or (not windows and info["format"] != "ELF64"):
            errors.append(f"{name}: executable format disagrees with SDK platform")
        if expected_machine is None or info["machine"] != expected_machine:
            errors.append(f"{name}: architecture disagrees with SDK identity")
        for labels in info.get("version_requirements", {}).values():
            for label in labels:
                match = re.fullmatch(r"(GLIBC|GLIBCXX|CXXABI)_(\d+(?:\.\d+)+)", label)
                if match:
                    prefix, value = match.groups()
                    if prefix not in maxima or version(value) > version(maxima[prefix]):
                        maxima[prefix] = value
        if max_glibc and any(label.startswith("GLIBC_") and re.fullmatch(r"GLIBC_\d+(?:\.\d+)+", label)
                             and version(label[6:]) > version(max_glibc)
                             for labels in info.get("version_requirements", {}).values() for label in labels):
            errors.append(f"{name}: requires glibc newer than declared {max_glibc} baseline")
    host = host_identity()
    if require_os:
        distribution, release = require_os.split(":", 1)
        actual = host.get("os_release", {})
        if actual.get("ID") != distribution or actual.get("VERSION_ID") != release:
            errors.append(f"Baseline requires {require_os}; actual host is {actual.get('ID')}:{actual.get('VERSION_ID')}")
    resolved = {}
    if windows:
        root_dlls = {}
        for path in directory.glob("*.dll"):
            if path.name.lower() in root_dlls:
                errors.append(f"Case-colliding DLL filenames: {path.name}")
            root_dlls[path.name.lower()] = path.name
        system = {p.name.lower() for p in system_directory.iterdir() if p.is_file()} if system_directory else set()
        for name, info in details.items():
            for needed in info["needed"]:
                if needed in root_dlls:
                    resolved[needed] = "sdk/" + root_dlls[needed]
                elif needed.startswith(("api-ms-win-", "ext-ms-win-")):
                    resolved[needed] = "Windows API-set contract (not minimum-OS proof)"
                elif needed in system:
                    resolved[needed] = "system/" + needed
                else:
                    errors.append(f"{name}: unresolved DLL {needed}; provide actual --system-directory for Windows system dependencies")
        if host_check and os.name != "nt":
            errors.append("Windows host check requires native Windows")
    elif host_check:
        if not sys.platform.startswith("linux"):
            errors.append("ELF host dependency check requires Linux")
        else:
            for name in manifest["tools"]:
                # Only explicitly requested for the verified, trusted SDK. ldd
                # is not a safe loader for arbitrary downloaded executables.
                result = subprocess.run(["ldd", str((directory / name).resolve())], capture_output=True, text=True, timeout=10)
                output = result.stdout + result.stderr
                details[name]["host_ldd"] = output
                if result.returncode or "not found" in output or "version `" in output:
                    errors.append(f"{name}: host loader reports unresolved/incompatible dependencies")
                for line in output.splitlines():
                    match = re.match(r"\s*(\S+)\s+=>\s+(/\S+)", line)
                    if match:
                        resolved[match[1]] = match[2]
    return {"schema": 1, "kind": "judas-sdk-qualification", "build": manifest["build"],
            "sdk_manifest_sha256": package_sdk.digest(directory / package_sdk.MANIFEST),
            "host": host, "policy": {"max_glibc": max_glibc, "require_os": require_os,
            "host_check": host_check, "windows_system_directory": str(system_directory) if system_directory else None},
            "tool_count": len(manifest["tools"]), "binaries": details,
            "highest_required_abi": maxima, "resolved_dependencies": resolved,
            "errors": errors, "passed": not errors,
            "scope": "ABI/import metadata and requested native loader checks; no graphics, application or hardware acceptance"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--host-check", action="store_true", help="Use ldd only on a trusted SDK; Windows still needs --system-directory")
    parser.add_argument("--max-glibc", help="Declared Linux binary baseline, for example 2.39")
    parser.add_argument("--require-os", help="Require actual qualification host ID:VERSION_ID, for example ubuntu:24.04")
    parser.add_argument("--system-directory", type=Path, help="Native Windows System32 for import resolution")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output, directory = args.output.resolve(), args.directory.resolve()
    if args.output.is_symlink() or output == directory or directory in output.parents:
        parser.error("Qualification receipt must be outside the verified SDK; its payload must remain unchanged")
    try:
        if args.max_glibc:
            version(args.max_glibc)
        if args.require_os and not re.fullmatch(r"[a-z0-9_-]+:[0-9.]+", args.require_os):
            raise ValueError("Use ID:VERSION_ID for --require-os")
        result = inspect_sdk(args.directory.resolve(), host_check=args.host_check, max_glibc=args.max_glibc,
                             require_os=args.require_os, system_directory=args.system_directory)
    except (ValueError, OSError, KeyError, subprocess.SubprocessError) as error:
        result = {"schema": 1, "kind": "judas-sdk-qualification", "passed": False, "errors": [str(error)]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"passed": result["passed"], "errors": result["errors"], "output": str(args.output)}))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
