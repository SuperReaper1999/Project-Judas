#!/usr/bin/env python3
"""Focused M90 Linux checks; serial workloads and raw per-command receipts.

Run from any working directory. --build rebuilds only the listed targets.
--graphics additionally requires a native OpenGL display. No production-suite
dispatcher, source mutation, Git operation or platform-certification claim.
"""
import argparse
import datetime
import json
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
NATIVE = [
    "judas_origin_physics_tests", "judas_origin_assembly_tests",
    "judas_origin_stream_tests", "judas_origin_script_tests", "judas_origin_audio_tests",
    "judas_character_motor_tests", "judas_wheel_tests",
    "judas_powertrain_tests", "judas_wheel_lifecycle_tests",
    "judas_powertrain_lifecycle_tests", "judas_navigation_tests",
    "judas_streaming_tests",
    "judas_save_exclusion_tests", "judas_save_storage_tests", "judas_physics_continuation_tests",
    "judas_physical_animation_tests", "judas_physical_balance_foundation_tests",
    "judas_physical_balance_controller_tests", "judas_section_material_tests",
    "judas_gravity_selection_tests", "judas_local_multiplayer_script_tests",
]
GRAPHICS = ["judas_origin_participant_tests", "judas_origin_travel_application_tests"]

def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument("--build-dir", default="build")
    p.add_argument("--output", default=".cache/m90/final-linux")
    p.add_argument("--build", action="store_true")
    p.add_argument("--graphics", action="store_true")
    p.add_argument("--navigation-project", help="optional owned dependency-test fixture")
    p.add_argument("--terrain-source", default="Imports/landscape.judasterrain")
    a = p.parse_args()
    build = (ROOT / a.build_dir).resolve()
    out = (ROOT / a.output).resolve()
    out.mkdir(parents=True, exist_ok=True)
    targets = NATIVE + (["judas_navigation_dependency_tests"] if a.navigation_project else []) + (GRAPHICS if a.graphics else [])
    receipts = []

    def run(name, command):
        started = datetime.datetime.now(datetime.timezone.utc).isoformat()
        before = time.monotonic()
        with (out / (name + ".log")).open("w") as log:
            result = subprocess.run(command, cwd=ROOT, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=1200)
        receipts.append(dict(name=name, command=list(map(str, command)),
                             cwd=str(ROOT), startedUTC=started,
                             seconds=time.monotonic()-before, exit=result.returncode))
        (out / "commands.json").write_text(json.dumps(receipts, indent=2)+"\n")
        print(name, "PASS" if result.returncode == 0 else "FAIL", flush=True)
        return result.returncode == 0

    if a.build and not run("build", ["cmake", "--build", str(build), "--target",
                                    *targets, "-j4"]):
        return 1
    okay = True
    for target in targets:
        args = [str(build / target)]
        if target == "judas_navigation_dependency_tests":
            args += [a.navigation_project, a.terrain_source]
        if target == "judas_save_exclusion_tests":
            args += [str(out / ("exclusion-"+str(time.time_ns()))), str(ROOT)]
        if target == "judas_save_storage_tests":
            args += [str(out / "m61-storage-m90")]
        if target == "judas_origin_travel_application_tests":
            args += [str(ROOT / "projects/travelling_lab/travelling_lab.judasproj"),
                     str(out / "travel")]
        okay = run(target, args) and okay
    return 0 if okay else 1

if __name__ == "__main__":
    sys.exit(main())
