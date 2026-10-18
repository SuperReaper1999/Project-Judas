#!/usr/bin/env python3
"""Prepare exact, hash-verified source deltas for an existing native build cache.

Does not contact the VM or change source/build files. Full source archives remain
the authority; the small transport ZIP avoids resending unchanged dependencies.
"""

import argparse
import hashlib
import json
from pathlib import Path
import tarfile
import zipfile


METADATA = ("JUDAS_SOURCE_INFO.json", "JUDAS_SOURCE_FILES.json")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_archive(path):
    with tarfile.open(path, "r:gz") as archive:
        contents = {}
        for entry in archive:
            if not entry.isfile() or entry.name.startswith("/") or ".." in Path(entry.name).parts:
                raise ValueError("Non-file or unsafe source entry: " + entry.name)
            if entry.name in contents:
                raise ValueError("Duplicate source entry: " + entry.name)
            contents[entry.name] = archive.extractfile(entry).read()
    info = json.loads(contents[METADATA[0]])
    inventory = json.loads(contents[METADATA[1]])
    if info["source_dirty"] or info["source_files_sha256"] != sha(contents[METADATA[1]]):
        raise ValueError("Source archive provenance/manifest disagreement")
    rows = inventory["files"]
    expected = {row["path"] for row in rows} | set(METADATA)
    if set(contents) != expected:
        raise ValueError("Source archive path inventory mismatch")
    for row in rows:
        if sha(contents[row["path"]]) != row["sha256"]:
            raise ValueError("Source archive hash mismatch: " + row["path"])
    return contents, info, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", required=True, type=Path)
    parser.add_argument("--after", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError("Preserve previous delta rather than overwrite")
    before, before_info, before_rows = read_archive(args.before)
    after, after_info, after_rows = read_archive(args.after)
    changed = sorted(name for name in after if before.get(name) != after[name])
    removed = sorted(set(before) - set(after))
    transport = {
        "schema": 1,
        "beforeArchiveSHA256": sha(args.before.read_bytes()),
        "afterArchiveSHA256": sha(args.after.read_bytes()),
        "beforeInfo": before_info,
        "afterInfo": after_info,
        "beforeFiles": before_rows,
        "afterFiles": after_rows,
        "changed": [{"path": name, "sha256": sha(after[name])} for name in changed],
        "removed": removed,
        "metadataBefore": {name: sha(before[name]) for name in METADATA},
        "metadataAfter": {name: sha(after[name]) for name in METADATA},
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("SOURCE_DELTA.json", json.dumps(transport, indent=2) + "\n")
        for name in changed:
            archive.writestr("payload/" + name, after[name])
    receipt = {
        "schema": 1,
        "path": str(args.output.resolve()),
        "sha256": sha(args.output.read_bytes()),
        "bytes": args.output.stat().st_size,
        "beforeInfo": before_info,
        "afterInfo": after_info,
        "changed": changed,
        "removed": removed,
    }
    args.output.with_suffix(".receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
