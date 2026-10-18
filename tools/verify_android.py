#!/usr/bin/env python3
"""Independently check Judas APK content/native provenance without installing it.

This verifies internal inventories, not publisher authenticity or Android runtime
behaviour. Signature and APK page alignment need apksigner/zipalign separately.
"""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import sys
import zipfile

# Packaged tools must not add Python caches to the checksum-verified SDK.
sys.dont_write_bytecode = True

import binary_dependencies
from package_sdk import digest, validate_identity

ABI_MACHINE = {"arm64-v8a": 183, "x86_64": 62}
# Conservative Android public NDK libraries supported by this runtime, all
# available at the declared API-26 floor. The builder also checks actual NDK
# API-26 stubs; arbitrary vendor/private libraries are not implied supported.
ANDROID_SYSTEM_LIBRARIES = frozenset(("libc.so", "libdl.so", "libm.so", "liblog.so", "libandroid.so",
    "libEGL.so", "libGLESv1_CM.so", "libGLESv2.so", "libGLESv3.so", "libOpenSLES.so", "libz.so",
    "libaaudio.so", "libcamera2ndk.so", "libmediandk.so", "libnativewindow.so", "libjnigraphics.so", "libvulkan.so"))


def safe_name(name):
    path = PurePosixPath(name)
    if not name or path.is_absolute() or ".." in path.parts or "\\" in name or path.as_posix() != name:
        raise ValueError(f"Unsafe APK inventory path {name!r}")
    return name


def verify_apk(path, expected_identities=None, receipt=None, max_unpacked=2 * 1024**3):
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        names = [entry.filename for entry in entries]
        if len(names) != len(set(names)):
            raise ValueError("Duplicate APK entries")
        if sum(entry.file_size for entry in entries) > max_unpacked:
            raise ValueError("APK exceeds the declared unpacked verification budget")
        for entry in entries:
            safe_name(entry.filename.rstrip("/"))
        def metadata(name):
            info = archive.getinfo(name)
            if info.file_size > 4 * 1024**2:
                raise ValueError(f"APK metadata exceeds bounded size: {name}")
            return json.loads(archive.read(name))

        identity = metadata("assets/judas-build-info.json")
        if identity.get("schema") != 1 or identity.get("kind") != "judas-android" or not identity.get("abis"):
            raise ValueError("Invalid APK engine identity")
        identities = identity["abis"]
        if any(abi not in ABI_MACHINE for abi in identities):
            raise ValueError("Unsupported APK ABI")
        # Android's CMake toolchain reports the ABI name arm64-v8a, while
        # other native metadata may use the equivalent CPU names. ELF machine
        # checks below independently establish the actual binary architecture.
        architectures = {"arm64-v8a": ("arm64-v8a", "aarch64", "arm64"), "x86_64": ("x86_64", "amd64")}
        for abi, value in identities.items():
            validate_identity(value)
            if value["platform"] != "Android" or value["architecture"].lower() not in architectures[abi]:
                raise ValueError("APK native identity must identify Android and its actual ABI architecture")
        common = ("version", "channel", "source_revision", "source_tree", "source_dirty", "source_fingerprint", "source_origin")
        first = next(iter(identities.values()))
        if first["source_origin"] == "git":
            common += ("source_untracked_count", "source_untracked_fingerprint")
        if any(any(value[field] != first[field] for field in common) for value in identities.values()):
            raise ValueError("APK ABIs have different source/version/channel identities")
        if expected_identities is not None and identities != expected_identities:
            raise ValueError("APK embedded identity disagrees with the build candidate")
        content = metadata("assets/judas-content.json")
        native = metadata("assets/judas-native.json")
        if content.get("schema") != 1 or native.get("schema") != 1:
            raise ValueError("Unsupported APK content/native inventory")

        def check(records, prefix, actual):
            expected = set()
            for record in records:
                name = prefix + safe_name(record["path"])
                if name in expected:
                    raise ValueError("Duplicate APK inventory record")
                expected.add(name)
                payload = archive.read(name)
                if len(payload) != record["bytes"] or hashlib.sha256(payload).hexdigest() != record["sha256"]:
                    raise ValueError(f"APK checksum/size mismatch: {name}")
            if expected != actual:
                raise ValueError(f"APK inventory omits or invents files: {sorted(expected ^ actual)}")

        check(content["files"], "assets/content/", {name for name in names if name.startswith("assets/content/") and not name.endswith("/")})
        check(native["files"], "", {name for name in names if name.startswith("lib/") and not name.endswith("/")})
        libraries = {}
        for abi in identities:
            expected = {f"lib/{abi}/{name}" for name in ("libmain.so", "libSDL2.so", "libc++_shared.so")}
            actual = {name for name in names if name.startswith(f"lib/{abi}/") and not name.endswith("/")}
            if expected != actual:
                raise ValueError(f"APK native library closure differs for {abi}")
            for name in sorted(actual):
                info = archive.getinfo(name)
                if info.compress_type != zipfile.ZIP_STORED:
                    raise ValueError(f"APK native library must be uncompressed: {name}")
                binary = binary_dependencies.elf(archive.read(name))
                if binary["machine"] != ABI_MACHINE[abi] or not binary["load_alignments"] or any(alignment < 16384 or alignment & (alignment - 1) for alignment in binary["load_alignments"]):
                    raise ValueError(f"APK native architecture/16 KiB ELF alignment failure: {name}")
                missing = set(binary["needed"]) - ANDROID_SYSTEM_LIBRARIES - {PurePosixPath(path).name for path in actual}
                if missing:
                    raise ValueError(f"APK native imports are unshipped or not supported Android public libraries: {name}: {sorted(missing)}")
                libraries[name] = binary
        if {name.split("/")[1] for name in names if name.startswith("lib/") and not name.endswith("/")} != set(identities):
            raise ValueError("APK contains native ABIs absent from engine identity")
    sha256 = digest(path)
    if receipt:
        if receipt.get("sha256") != sha256 or receipt.get("engine") != identities or receipt.get("shippedNative") != native["files"]:
            raise ValueError("APK external receipt disagrees with shipped bytes/identity")
    return {"schema": 1, "kind": "judas-android-verification", "passed": True,
            "sha256": sha256, "bytes": path.stat().st_size, "engine": identities,
            "contentFiles": len(content["files"]), "contentBytes": sum(entry["bytes"] for entry in content["files"]),
            "native": libraries, "shippedNative": native["files"],
            "scope": "ZIP/content/native-byte/ELF verification only; signatures, ZIP alignment and device execution are separate"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.output and (args.output.is_symlink() or args.output.resolve() in {
            args.apk.resolve(), args.apk.with_suffix('.build.json').resolve(), args.apk.with_suffix('.sha256').resolve(),
            *([args.receipt.resolve()] if args.receipt else [])}):
        parser.error("Verification output must not overwrite the APK or its existing receipt/checksum")
    try:
        result = verify_apk(args.apk, receipt=json.loads(args.receipt.read_text()) if args.receipt else None)
    except (ValueError, KeyError, OSError, zipfile.BadZipFile) as error:
        result = {"schema": 1, "kind": "judas-android-verification", "passed": False, "error": str(error)}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, sort_keys=True))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
