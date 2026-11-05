#!/usr/bin/env python3
"""Compile the particle observer against the preserved pre-particle-V2 ABI.

This does not rebuild or run the engine. The observer is copied verbatim, while
all engine headers, the engine archive, build identity and GL loader come from
the frozen baseline. No current src directory is used as an include fallback.
"""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[2]
    parser.add_argument("--baseline", type=Path, default=root / ".cache/particles-v2/baseline")
    parser.add_argument("--build", type=Path, default=root / "build")
    parser.add_argument("--observer", type=Path, default=root / "tests/ParticlesV2Benchmark.cpp")
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    baseline, build, observer = (value.resolve() for value in (args.baseline, args.build, args.observer))
    original = json.loads((baseline / "PROVENANCE.json").read_text(encoding="utf-8"))
    for name in ("source.tar", "libjudas_engine.a", "libjudas_build_identity.a", "libjudas_glad.a"):
        if digest(baseline / name) != original["files"][name]:
            raise RuntimeError(f"frozen baseline hash changed: {name}")
    tree = baseline / "tree"
    marker = tree / "BASELINE_SOURCE_SHA256"
    if tree.exists():
        if not marker.is_file() or marker.read_text().strip() != original["files"]["source.tar"]:
            raise RuntimeError("existing baseline/tree has no matching extraction receipt")
    else:
        tree.mkdir()
        with tarfile.open(baseline / "source.tar") as archive:
            archive.extractall(tree, filter="data")
        marker.write_text(original["files"]["source.tar"] + "\n")
    if (tree / "src/particles/GpuParticles.h").exists():
        raise RuntimeError("frozen baseline unexpectedly exposes the candidate GPU API")
    (tree / "tests").mkdir(exist_ok=True)
    (tree / "third_party/nlohmann").mkdir(parents=True, exist_ok=True)
    frozen_observer = tree / "tests/ParticlesV2Benchmark.cpp"
    frozen_json = tree / "third_party/nlohmann/json.hpp"
    shutil.copy2(observer, frozen_observer)
    shutil.copy2(root / "third_party/nlohmann/json.hpp", frozen_json)

    # These unchanged support libraries are explicitly snapshotted and hashed
    # separately. They were not part of the original baseline archive receipt;
    # never describe their hashes as having been recorded at baseline capture.
    support = baseline / "observer-support"
    support.mkdir(exist_ok=True)
    library_names = [
        "libjudas_graphics_caps.a", "libjudas_app_icon.a", "libjudas_materials.a",
        "libjudas_audio_backend.a", "libjudas_meshoptimizer.a",
        "libjudas_navigation_cook.a", "libjudas_navigation_backend.a",
        "libjudas_quickjs.a", "libjudas_profiler.a", "libjudas_jobs.a",
        "_deps/judas_freetype-build/libfreetype.a", "_deps/judas_harfbuzz-build/libharfbuzz.a",
        "unicode/icu/lib/libicui18n.a", "unicode/icu/lib/libicuuc.a", "unicode/icu/lib/libicudata.a",
    ]
    support_libraries = {}
    for name in library_names:
        source = build / name
        destination = support / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists() and digest(destination) != digest(source):
            raise RuntimeError(f"support archive changed after first observer preparation: {name}")
        if not destination.exists():
            shutil.copy2(source, destination)
        support_libraries[name] = digest(destination)
    obj = baseline / "ParticlesV2Benchmark.o"
    binary = baseline / "judas_particles_v2_benchmark"
    dependencies = baseline / "ParticlesV2Benchmark.d"
    compiler = shutil.which("c++")
    if not compiler:
        raise RuntimeError("C++ compiler unavailable")
    compile_command = [compiler, "-O3", "-DNDEBUG", "-std=gnu++17", "-Wall", "-Wextra",
        "-DDT_VIRTUAL_QUERYFILTER", "-DMA_NO_ENCODING", "-DMA_NO_GENERATION", "-DMA_NO_RESOURCE_MANAGER",
        "-I" + str(tree / "src"), "-I" + str(tree / "third_party/glad/include"),
        "-isystem", "/usr/include/SDL2", "-MMD", "-MF", str(dependencies),
        "-c", str(frozen_observer), "-o", str(obj)]
    link_command = [compiler, "-O3", "-DNDEBUG", str(obj), "-o", str(binary),
        str(baseline / "libjudas_engine.a"), str(support / "libjudas_graphics_caps.a"),
        str(baseline / "libjudas_build_identity.a"), str(support / "libjudas_app_icon.a"),
        str(support / "libjudas_materials.a"), "/usr/lib/x86_64-linux-gnu/libSDL2.so",
        str(baseline / "libjudas_glad.a")]
    link_command += [str(support / name) for name in library_names[3:8]]
    link_command += ["-lm", str(support / "libjudas_profiler.a")]
    link_command += [str(support / name) for name in library_names[10:]]
    link_command += ["-ldl", str(support / "libjudas_jobs.a")]
    receipt = {
        "schema": 1, "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "revision": original["revision"], "baseline_capture": original,
        "observer_sha256": digest(frozen_observer), "json_dependency_sha256": digest(frozen_json),
        "script_sha256": digest(Path(__file__)), "support_archives_sha256": support_libraries,
        "support_archive_note": "Copied from the unchanged support build at observer preparation, not recorded at baseline capture.",
        "compile": compile_command, "link": link_command, "prepared_only": args.prepare_only,
    }
    if not args.prepare_only:
        environment = dict(os.environ)
        for name in ("CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "OBJC_INCLUDE_PATH"):
            environment.pop(name, None)
        with (baseline / "observer-build.log").open("w", encoding="utf-8") as log:
            for command in (compile_command, link_command):
                subprocess.run(command, cwd=tree, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)
        if str(root / "src") in dependencies.read_text(encoding="utf-8"):
            raise RuntimeError("observer accidentally used current engine headers")
        receipt["binary_sha256"] = digest(binary)
        receipt["dependencies_sha256"] = digest(dependencies)
    (baseline / "OBSERVER_BUILD.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print("Prepared" if args.prepare_only else "Built", binary)


if __name__ == "__main__":
    main()
