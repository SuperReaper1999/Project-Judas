"""Task-local native cache update with exact pre/post source verification.

The accepted source archive, native PEs and pristine export remain preserved.
Only inventoried source files in the existing task-owned cache are updated.
"""

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import time
import zipfile


TASK = Path(r"P:\JudasValidation\PreAlphaPass5")
PREVIOUS = Path(r"P:\JudasValidation\PreAlphaPass4")
SOURCE = PREVIOUS / "Source"
METADATA = ("JUDAS_SOURCE_INFO.json", "JUDAS_SOURCE_FILES.json")


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def safe(name):
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or "\\" in name or ":" in name:
        raise ValueError("Unsafe source-delta path: " + name)
    return path.as_posix()


def verify_source(rows, metadata):
    expected = {safe(row["path"]) for row in rows} | set(METADATA)
    actual = {path.relative_to(SOURCE).as_posix() for path in SOURCE.rglob("*") if path.is_file()}
    if actual != expected:
        raise ValueError("Source path inventory mismatch: " + repr(sorted(actual ^ expected)))
    for row in rows:
        if sha(SOURCE / row["path"]) != row["sha256"]:
            raise ValueError("Source hash mismatch: " + row["path"])
    for name, expected_hash in metadata.items():
        if sha(SOURCE / name) != expected_hash:
            raise ValueError("Source metadata mismatch: " + name)


def main():
    receipt = json.loads((TASK / "source-delta.receipt.json").read_text(encoding="utf-8"))
    delta = TASK / "source-delta.zip"
    if sha(delta) != receipt["sha256"]:
        raise ValueError("Transferred delta hash mismatch")
    before = TASK / "SourceBefore"
    if before.exists():
        raise ValueError("Preserve prior update attempt rather than overwrite")
    with zipfile.ZipFile(delta) as archive:
        spec = json.loads(archive.read("SOURCE_DELTA.json"))
        payload = {"payload/" + safe(row["path"]) for row in spec["changed"]}
        if set(archive.namelist()) != payload | {"SOURCE_DELTA.json"}:
            raise ValueError("Delta path inventory mismatch")
        for row in spec["changed"]:
            if hashlib.sha256(archive.read("payload/" + row["path"])).hexdigest() != row["sha256"]:
                raise ValueError("Delta payload mismatch: " + row["path"])
        verify_source(spec["beforeFiles"], spec["metadataBefore"])
        original_archive = PREVIOUS / "final-source.tar.gz"
        if sha(original_archive) != spec["beforeArchiveSHA256"]:
            raise ValueError("Original accepted source archive is not preserved")
        binaries = json.loads((PREVIOUS / "NATIVE_BINARIES.json").read_text(encoding="utf-8-sig"))["files"]
        pristine = PREVIOUS / ("Moved Game " + chr(0xe9) + " " + chr(0x3b1))
        package = json.loads((PREVIOUS / "PRISTINE_PACKAGE_FILES.json").read_text(encoding="utf-8-sig"))
        for row in binaries:
            if sha(Path(row["path"])) != row["sha256"]:
                raise ValueError("Accepted native binary changed before cache reuse: " + row["path"])
        for row in package:
            if sha(pristine / row["path"]) != row["sha256"]:
                raise ValueError("Accepted pristine export changed: " + row["path"])
        before.mkdir()
        # Preserve exact accepted PEs before their disposable build-cache copies
        # are regenerated. Stock accepted SDK/export trees are never build outputs.
        preserved_binaries = TASK / "AcceptedPass4Binaries"
        preserved_binaries.mkdir()
        binary_rows = []
        for index, row in enumerate(binaries):
            destination = preserved_binaries / (str(index) + "-" + Path(row["path"]).name)
            shutil.copy2(row["path"], destination)
            if sha(destination) != row["sha256"]:
                raise ValueError("Native PE preservation failed")
            binary_rows.append({**row, "preservedAt": str(destination)})
        changed_paths = [safe(row["path"]) for row in spec["changed"]]
        for name in sorted(set(changed_paths) | {safe(name) for name in spec["removed"]}):
            old = SOURCE / name
            if old.exists():
                target = before / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(old, target)
                if sha(old) != sha(target):
                    raise ValueError("Source backup mismatch: " + name)
            if name in spec["removed"]:
                old.unlink()
        for name in changed_paths:
            target = SOURCE / name
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.with_name(target.name + ".pass5-new")
            temporary.write_bytes(archive.read("payload/" + name))
            os.replace(temporary, target)
        verify_source(spec["afterFiles"], spec["metadataAfter"])
    output = {
        "schema": 1, "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "source": str(SOURCE), "build": str(PREVIOUS / "Build"),
        "beforeInfo": spec["beforeInfo"], "afterInfo": spec["afterInfo"],
        "beforeFilesVerified": len(spec["beforeFiles"]),
        "afterFilesVerified": len(spec["afterFiles"]),
        "changedPaths": changed_paths, "removedPaths": spec["removed"],
        "acceptedNativeBinariesPreserved": binary_rows,
        "acceptedPristineExportUnchanged": True,
        "originalAcceptedSourceArchive": str(original_archive),
        "originalAcceptedSourceArchiveSHA256": spec["beforeArchiveSHA256"],
        "cacheReuse": "Existing CMake source/build paths; no cache/generated-path rewriting",
        "passed": True,
    }
    (TASK / "SOURCE_VERIFIED.json").write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": True, "files": len(spec["afterFiles"]), "changed": len(changed_paths)}))


if __name__ == "__main__":
    main()
