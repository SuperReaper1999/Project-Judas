#!/usr/bin/env python3
"""Create a deterministic tracked-source archive with verified no-Git build provenance.

The default engine profile excludes games, protected evidence and independent
research. --profile full explicitly includes every tracked file, potentially GiB.
This command never changes the source checkout, index, refs or untracked files.
"""

import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
METADATA = {"JUDAS_SOURCE_INFO.json", "JUDAS_SOURCE_FILES.json"}
ENGINE_ROOTS = {"cmake", "src", "tests", "tools", "scripts", "platform", "packaging", "third_party", "assets", "releases", ".github"}
ENGINE_FILES = {"AGENTS.md", "CMakeLists.txt", "CMakePresets.json", "CTestConfig.cmake", "README.md", "LICENSE", "engine-version.json", ".gitignore", ".gitattributes"}
EXCLUDED_GUIDES = {"docs/DontReadMe_Final.md", "docs/FTFT.md"}


def git(source, *args):
    return subprocess.check_output(["git", "-C", str(source), *args])


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def selected(name, profile):
    if name in METADATA:
        return False
    if profile == "full":
        return True
    path = PurePosixPath(name)
    return (name in ENGINE_FILES or path.parts[0] in ENGINE_ROOTS or
            name.startswith("docs/judasjs/") or name == "docs/judas.d.ts" or
            len(path.parts) == 2 and path.parts[0] == "docs" and
            path.suffix in (".md", ".json") and name not in EXCLUDED_GUIDES)


def safe_path(name):
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or any(c in name for c in ('\\', '"', '\n', '\r', ';', '\t', '[', ']')):
        raise ValueError(f"Source path cannot be represented safely: {name!r}")
    return name


def archive(args):
    source = args.source.resolve()
    requested = args.output.absolute()
    companions = [requested, requested.with_name(requested.name + ".build.json"), requested.with_name(requested.name + ".sha256")]
    if any(path.is_symlink() for path in companions):
        raise ValueError("Source archive outputs must not be symlinks")
    output = requested.resolve()
    if output == source or source in output.parents:
        raise ValueError("Source archive output must be outside the source checkout")
    if any(path.exists() for path in companions) and not args.replace:
        raise ValueError("Archive or receipt/checksum already exists; use another name or explicit --replace")
    if Path(git(source, "rev-parse", "--show-toplevel").decode().strip()).resolve() != source:
        raise ValueError("Source must be the exact Git checkout root")
    revision = git(source, "rev-parse", "HEAD").decode().strip()
    tree = git(source, "rev-parse", "HEAD^{tree}").decode().strip()
    if git(source, "diff", "HEAD", "--binary", "--no-ext-diff"):
        raise ValueError("Tracked source is dirty; checkpoint the intended candidate before archiving")
    untracked = [name for name in git(source, "ls-files", "--others", "--exclude-standard", "-z").split(b"\0") if name]
    if untracked and not args.allow_untracked:
        raise ValueError("Untracked files exist; commit intended inputs or explicitly --allow-untracked to exclude them")
    branch = git(source, "rev-parse", "--abbrev-ref", "HEAD").decode().strip()
    maturity = json.loads(git(source, "show", "HEAD:engine-version.json"))["maturity"]
    release_channel = {"alpha": "Alpha", "beta": "Beta", "stable": "Stable"}.get(maturity, "Experimental")
    channel = args.channel or ({"main": release_channel, "Experimental": "Experimental"}.get(branch, "Local"))
    entries = git(source, "ls-tree", "-r", "-z", "HEAD").split(b"\0")
    blobs = {}
    for entry in entries:
        if not entry:
            continue
        info, encoded = entry.split(b"\t", 1)
        name = safe_path(encoded.decode("utf-8"))
        if not selected(name, args.profile):
            continue
        mode, kind, blob = info.decode().split()
        if kind != "blob" or mode not in ("100644", "100755"):
            raise ValueError(f"Unsupported source entry (submodule/symlink): {name}")
        blobs[name] = (blob, mode)
    if not blobs:
        raise ValueError("No selected source files")
    # An index mapping paths to exact Git blobs also prevents export-subst or
    # export-ignore attributes from silently changing the selected source bytes.
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
    if epoch < 0:
        raise ValueError("SOURCE_DATE_EPOCH cannot be negative")
    output.parent.mkdir(parents=True, exist_ok=True)
    records = []
    with tempfile.TemporaryDirectory(prefix="judas-source-", dir=output.parent) as temporary:
        prepared = Path(temporary) / "source.tar.gz"
        batch = subprocess.Popen(["git", "-C", str(source), "cat-file", "--batch"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        try:
            with prepared.open("wb") as stream, gzip.GzipFile(filename="", mode="wb", fileobj=stream, mtime=epoch) as compressed:
                with tarfile.open(fileobj=compressed, mode="w|", format=tarfile.PAX_FORMAT) as target:
                    for name in sorted(blobs):
                        blob, mode = blobs[name]
                        batch.stdin.write((blob + "\n").encode())
                        batch.stdin.flush()
                        header = batch.stdout.readline().decode().split()
                        if len(header) != 3 or header[1] != "blob":
                            raise ValueError(f"Cannot read Git source blob: {name}")
                        size = int(header[2])
                        # Source blobs are streamed, including large vendored archives.
                        hasher = hashlib.sha256()

                        class Reader:
                            def read(self, amount):
                                data = batch.stdout.read(amount)
                                hasher.update(data)
                                return data

                        member = tarfile.TarInfo(name)
                        member.size = size
                        member.mode = 0o755 if mode == "100755" else 0o644
                        member.mtime = epoch
                        target.addfile(member, Reader())
                        if batch.stdout.read(1) != b"\n":
                            raise ValueError("Git blob stream lost its boundary")
                        records.append({"path": name, "sha256": hasher.hexdigest()})
                    manifest = ('{"schema":1,"files":[\n' +
                                ',\n'.join(json.dumps(record, ensure_ascii=False, separators=(",", ":")) for record in records) + '\n]}\n').encode()
                    info = {"schema": 1, "revision": revision, "tree": tree, "source_dirty": False,
                            "source_fingerprint": hashlib.sha256((tree + "\n").encode()).hexdigest(),
                            "channel": channel, "source_files_sha256": hashlib.sha256(manifest).hexdigest(),
                            "selection": args.profile}
                    for name, data in (("JUDAS_SOURCE_FILES.json", manifest),
                                       ("JUDAS_SOURCE_INFO.json", (json.dumps(info, sort_keys=True, indent=2) + "\n").encode())):
                        member = tarfile.TarInfo(name)
                        member.size, member.mode, member.mtime = len(data), 0o644, epoch
                        target.addfile(member, io.BytesIO(data))
            batch.stdin.close()
            if batch.wait(timeout=10) != 0:
                raise ValueError("Git blob reader failed")
            if git(source, "rev-parse", "HEAD").decode().strip() != revision or git(source, "diff", "HEAD", "--binary", "--no-ext-diff"):
                raise ValueError("Source changed while preparing archive; nothing published")
            os.replace(prepared, output)
        finally:
            if batch.poll() is None:
                batch.kill()
                batch.wait()
    report = {"schema": 1, "kind": "judas-source-archive", "file": output.name,
              "sha256": digest(output), "bytes": output.stat().st_size, "source": info,
              "files": len(records), "untrackedExcluded": len(untracked),
              "scope": "engine build/source and current docs" if args.profile == "engine" else "all tracked files"}
    receipt = output.with_name(output.name + ".build.json")
    receipt.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8", newline="\n")
    output.with_name(output.name + ".sha256").write_text(f"{report['sha256']}  {output.name}\n{digest(receipt)}  {receipt.name}\n", encoding="utf-8", newline="\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--channel", choices=["Alpha", "Beta", "Experimental", "Stable", "Local"])
    parser.add_argument("--profile", choices=["engine", "full"], default="engine")
    parser.add_argument("--allow-untracked", action="store_true")
    parser.add_argument("--replace", action="store_true")
    args = parser.parse_args()
    try:
        print(json.dumps(archive(args), sort_keys=True))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Source packaging failed: {error}\n")


if __name__ == "__main__":
    main()
