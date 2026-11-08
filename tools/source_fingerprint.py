#!/usr/bin/env python3
"""Stream the tracked-source provenance input; never retain a full Git diff."""

import argparse
import hashlib
import json
import subprocess
import tempfile


def fingerprint(git, root, tree):
    digest = hashlib.sha256((tree + "\n").encode("ascii"))
    dirty = False
    # Git warnings must not fill a pipe while its potentially large stdout is
    # streamed. Keep diagnostics on disk and report only a bounded error tail.
    with tempfile.TemporaryFile() as errors:
        with subprocess.Popen(
            [git, "-C", str(root), "diff", "HEAD", "--binary", "--no-ext-diff",
             "--full-index", "--no-color", "--no-renames", "--no-textconv",
             "--diff-algorithm=myers"],
            stdout=subprocess.PIPE, stderr=errors,
        ) as process:
            carry = b""
            while chunk := process.stdout.read(1024 * 1024):
                dirty = True
                chunk = carry + chunk
                carry = b"\r" if chunk.endswith(b"\r") else b""
                if carry:
                    chunk = chunk[:-1]
                # execute_process(OUTPUT_VARIABLE) normalizes CRLF. Preserve
                # that exact existing hash contract across read boundaries.
                digest.update(chunk.replace(b"\r\n", b"\n"))
            digest.update(carry)
            if process.wait():
                errors.seek(0)
                raise RuntimeError("Cannot fingerprint tracked source: " +
                                   errors.read(4096).decode("utf-8", errors="replace"))
    return {"source_fingerprint": digest.hexdigest(), "source_dirty": dirty}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("git")
    parser.add_argument("root")
    parser.add_argument("tree")
    args = parser.parse_args()
    print(json.dumps(fingerprint(args.git, args.root, args.tree)))


if __name__ == "__main__":
    main()
