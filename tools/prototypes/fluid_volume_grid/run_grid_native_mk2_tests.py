#!/usr/bin/env python3
"""Run the fluid-grid MK2 probes against an existing native GNU Make build."""

import argparse
import pathlib
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path, help="directory for fresh per-probe logs")
    args = parser.parse_args()
    build = args.build.resolve()
    output = args.output.resolve()
    target = build / "CMakeFiles/judas_production_fluid_tests.dir"
    flags_file, link_file = target / "flags.make", target / "link.txt"
    if not flags_file.is_file() or not link_file.is_file():
        parser.error("build must contain GNU Make files for judas_production_fluid_tests")

    suites = ("MovingGridMk2Tests", "MovingGridMk2ReconcileTests", "MovingGridMk2MixedTests")
    logs = [output / (suite + ".log") for suite in suites]
    if any(log.exists() for log in logs):
        parser.error("output already contains an MK2 probe log")
    output.mkdir(parents=True, exist_ok=True)
    compile_flags = []
    for line in flags_file.read_text().splitlines():
        key, _, values = line.partition(" = ")
        if key in {"CXX_DEFINES", "CXX_INCLUDES", "CXX_FLAGS"}:
            compile_flags.extend(shlex.split(values))
    link = [
        item
        for item in shlex.split(link_file.read_text())
        if not item.startswith("-Wl,--dependency-file=")
    ]
    originals = [i for i, item in enumerate(link) if item.endswith("ProductionFluidTests.cpp.o")]
    if len(originals) != 1 or "-o" not in link:
        parser.error("judas_production_fluid_tests link command does not name the expected object")

    source = pathlib.Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="judas-native-grid-mk2-") as temporary:
        scratch = pathlib.Path(temporary)

        def compile_source(name):
            obj = scratch / (name + ".o")
            subprocess.run(
                [link[0], *compile_flags, "-Werror", "-c", source / (name + ".cpp"), "-o", obj],
                cwd=build,
                check=True,
            )
            return obj

        shared = [
            compile_source(name)
            for name in ("MovingVolumeGrid", "MovingVolumeMomentum", "HalfDualVolumeMomentum", "CoupledVolumeBox",
                         "GridHalfCellGeometry", "GridGeometricMomentum", "GridConservativeRemap",
                         "GridPhaseFluxRepair")
        ]
        failed = False
        for suite, log in zip(suites, logs):
            command = link.copy()
            executable = scratch / suite
            command[command.index("-o") + 1] = executable
            command[originals[0]:originals[0] + 1] = [compile_source(suite), *shared]
            subprocess.run(command, cwd=build, check=True)
            with log.open("x") as report:
                result = subprocess.run([executable], stdout=report, stderr=subprocess.STDOUT)
            print(f"{suite} exit={result.returncode} log={log}", flush=True)
            failed = failed or result.returncode != 0
        return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
