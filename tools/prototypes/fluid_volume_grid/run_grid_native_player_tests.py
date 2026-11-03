#!/usr/bin/env python3
"""Relink the prototype consumer against an already built native GNU Make tree."""

import argparse
import pathlib
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=pathlib.Path)
    args = parser.parse_args()
    build = args.build.resolve()
    target = build / "CMakeFiles/judas_player_fluid_integration_tests.dir"
    compile_flags = []
    for line in (target / "flags.make").read_text().splitlines():
        key, _, values = line.partition(" = ")
        if key in {"CXX_DEFINES", "CXX_INCLUDES", "CXX_FLAGS"}:
            compile_flags.extend(shlex.split(values))
    link = [
        item
        for item in shlex.split((target / "link.txt").read_text())
        if not item.startswith("-Wl,--dependency-file=")
    ]
    source = pathlib.Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="judas-native-grid-player-") as temporary:
        out = pathlib.Path(temporary)
        objects = []
        for path in ("GridNativePlayerTests.cpp", "MovingVolumeGrid.cpp", "HalfDualVolumeMomentum.cpp"):
            obj = out / (path + ".o")
            subprocess.run(
                [link[0], *compile_flags, "-Werror", "-c", source / path, "-o", obj],
                cwd=build,
                check=True,
            )
            objects.append(obj)
        existing = next(
            i for i, item in enumerate(link) if item.endswith("PlayerFluidIntegrationTests.cpp.o")
        )
        link[existing:existing + 1] = objects
        executable = out / "judas_grid_native_player_tests"
        link[link.index("-o") + 1] = executable
        subprocess.run(link, cwd=build, check=True)
        subprocess.run([executable], check=True)


if __name__ == "__main__":
    main()
