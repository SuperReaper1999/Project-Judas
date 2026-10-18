#!/usr/bin/env python3
"""Verify exact JudasJS payload preservation and bounded C++ raw literals.

This is a pinned correction verifier, not a general C++ parser or API checker.
MSVC's actual compile/run controls are retained separately.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

EXPECTED_BYTES = 30305
EXPECTED_SHA256 = "b9d26c0405d234a31dd9a5f95754a5dd38ad69d46966dad76a915344ee838da2"
MAX_LITERAL_BYTES = 16380
MARKER = b"const char* library="
BOUNDARY = b')JS" R"JS(export class EntityGravity'


def extract(source):
    if source.count(MARKER) != 1:
        raise ValueError("expected one virtual-module initializer")
    expression = source.split(MARKER, 1)[1].split(b')JS";', 1)[0] + b')JS"'
    chunks = re.findall(rb'R"JS\((.*?)\)JS"', expression, re.S)
    remainder = re.sub(rb'R"JS\(.*?\)JS"', b"", expression, flags=re.S)
    if not chunks or remainder.strip():
        raise ValueError("virtual module must consist of adjacent raw literals")
    return chunks


def verify(source):
    chunks = extract(source)
    payload = b"".join(chunks)
    if len(payload) != EXPECTED_BYTES or hashlib.sha256(payload).hexdigest() != EXPECTED_SHA256:
        raise ValueError("virtual-module payload changed")
    if max(map(len, chunks)) > MAX_LITERAL_BYTES:
        raise ValueError("individual raw literal exceeds the MSVC byte limit")
    return {"chunk_bytes": list(map(len, chunks)), "payload_bytes": len(payload),
            "payload_sha256": hashlib.sha256(payload).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()
    current = args.source.read_bytes()
    result = verify(current)
    if current.count(BOUNDARY) != 1:
        raise ValueError("expected one corrected subsystem boundary")
    original = current.replace(BOUNDARY, b"export class EntityGravity", 1)
    old_chunks = extract(original)
    if b"".join(old_chunks) != b"".join(extract(current)):
        raise ValueError("concatenated JavaScript bytes are not identical")
    negatives = {
        "remerge_original_oversized_literal": original,
        "change_export_name": current.replace(b"export class EntityGravity", b"export class EntityGravita", 1),
        "insert_javascript_newline": current.replace(BOUNDARY, b')JS" R"JS(\nexport class EntityGravity', 1),
    }
    controls = {}
    for name, source in negatives.items():
        try:
            verify(source)
        except ValueError as failure:
            controls[name] = {"detected": True, "reason": str(failure)}
        else:
            raise ValueError("negative control failed to detect " + name)
    result.update(original_chunk_bytes=list(map(len, old_chunks)),
                  original_source_sha256=hashlib.sha256(original).hexdigest(),
                  corrected_source_sha256=hashlib.sha256(current).hexdigest(),
                  concatenated_bytes_identical=True, negative_controls=controls)
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
