#!/usr/bin/env python3
"""Exercise the real inventory assertion with missing reachable declarations."""
import argparse
import importlib.util
import io
import json
from pathlib import Path
import re
import sys
import unittest
from unittest.mock import patch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source_root = args.source.resolve()
    spec = importlib.util.spec_from_file_location(
        "packaging_checks", source_root / "scripts/prealpha_packaging_checks.py")
    checks = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(checks)
    original_scanner = checks.cmake_declared_targets
    source = (source_root / "CMakeLists.txt").read_text(encoding="utf-8")
    missing_helper, count = re.subn(
        r"judas_add_engine_test\(judas_windows_readiness_tests[^\n]*", "", source)
    if count != 1:
        raise RuntimeError("Expected exactly one real helper invocation")
    missing_include = source.replace("include(cmake/targets/RenderingAssets.cmake)", "")
    if missing_include == source:
        raise RuntimeError("Real module include was not removed")
    controls = (
        ("missing-helper", missing_helper, "judas_windows_readiness_tests"),
        ("unreachable-module", missing_include, "judas_cook"),
    )
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    for label, changed_source, expected_missing in controls:
        output = io.StringIO()
        def scan(root):
            return original_scanner(root, {"CMakeLists.txt": changed_source})
        with patch.object(checks, "cmake_declared_targets", scan):
            suite = unittest.TestSuite([checks.PackagingChecks(
                "test_shared_inventory_contains_every_current_public_tool")])
            result = unittest.TextTestRunner(stream=output, verbosity=2).run(suite)
        raw = output.getvalue()
        (args.output / (label + ".log")).write_text(raw, encoding="utf-8")
        detected = (result.testsRun == 1 and len(result.failures) == 1
                    and not result.errors and expected_missing in raw)
        rows.append({"control": label, "expected_missing": expected_missing,
                     "tests": result.testsRun, "failures": len(result.failures),
                     "errors": len(result.errors), "detected": detected})
    passed = all(row["detected"] for row in rows)
    (args.output / "RESULTS.json").write_text(
        json.dumps({"schema": 1, "passed": passed, "controls": rows}, indent=2) + "\n")
    print(json.dumps(rows, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
