#!/usr/bin/env python3
"""Build real MK2 with a private planar-joint experiment in a disposable source copy.

Production src/, CMake configuration and public APIs are never edited. Every
source transformation requires an exact single match, so source drift fails.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import os
from planar2d_reduced_copy import patch as patch_reduced


def change(path, old, new):
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise RuntimeError(f"Expected one patch anchor in {path}: {old[:90]}")
    path.write_text(text.replace(old, new), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(".cache/planar2d-native-stage0"))
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--reduced", action="store_true")
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    if args.baseline and args.reduced:
        parser.error("baseline and reduced modes are mutually exclusive")
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    source = output / "source"
    source.mkdir(parents=True, exist_ok=True)
    before = {p: (hashlib.sha256(p.read_bytes()).hexdigest(), p.stat().st_mtime_ns)
              for p in source.rglob("*") if p.is_file()}
    shutil.copytree(root / "src", source / "src", dirs_exist_ok=True)
    shutil.copytree(root / "third_party/quickhull", source / "third_party/quickhull", dirs_exist_ok=True)
    shutil.copytree(root / "tests", source / "tests", dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("fixtures", "*.csv", "*.json"))
    shutil.copy2(root / "tools/prototypes/planar2d_native_rows.cpp", source / "NativeRows.cpp")
    shutil.copy2(root / "tools/prototypes/planar2d_3d_control.cpp", source / "Control3D.cpp")
    shutil.copy2(root / "tools/prototypes/planar2d_3d_control.h", source / "planar2d_3d_control.h")
    change(source / "src/JointTypes.h", "enum class JointType {Fixed,Hinge,Ball,Slider};",
           "enum class JointType {Fixed,Hinge,Ball,Slider,Stage0Planar,Stage0Pin};")
    change(source / "src/JointTypes.h", "int(s.type)<=3", "int(s.type)<=5")
    change(source / "src/JointSolver.cpp", "  if(s.type==JointType::Slider){", """  if(s.type==JointType::Stage0Planar){
   // Experimental native rows: a fixed world plane through the COM. No
   // tangential restriction, no hidden carrier/anchor body and no pose clamps.
   const auto fixed=glm::mat3_cast(fb);
   const auto normal=fixed[2];
   row(-normal,normal,{0,0,0},{0,0,0},glm::dot(delta,normal));
   const auto error=glm::cross(basis[2],normal);
   for(int k=0;k<2;++k)angular(fixed[k],glm::dot(error,fixed[k]));
   continue;
  }
  if(s.type==JointType::Stage0Pin){
   // A real planar point constraint has TWO translational rows and no
   // approximate 3D angular alignment rows. Reuses the same native solve.
   for(int k=0;k<2;++k)linear(basis[k],glm::dot(delta,basis[k]));
   continue;
  }
  if(s.type==JointType::Slider){""")
    change(source / "src/physics/WorldSleep.cpp", "            if (s.type == JointType::Slider) {", """            if (s.type == JointType::Stage0Planar) {
                const auto normal = s.frameB * glm::vec3(0,0,1);
                error = normal * glm::dot(error,normal);
                const auto movingNormal = a->rigidBody.orientation * s.frameA * glm::vec3(0,0,1);
                island.quiet &= glm::length(glm::cross(normal,movingNormal)) < .01f;
            }
            if (s.type == JointType::Slider) {""")
    if args.reduced:
        patch_reduced(source, change)
        # Preserve a runnable counterexample to blind reuse of 3D hinge rows.
        # Same physics, with ONLY the new unsupported-joint guard removed.
        original_joints=(source / "src/physics/WorldJoints.cpp").read_text(encoding="utf-8")
        start=original_joints.index("    if(a->rigidBody.Stage0Planar()")
        end=original_joints.index("    static std::atomic<std::uint64_t> next{1};",start)
        (source / "LegacyJoints.cpp").write_text(original_joints[:start]+original_joints[end:],encoding="utf-8")
    if args.baseline:
        shutil.copytree(root / "src", source / "src", dirs_exist_ok=True)
    else:
        change(source / "src/PhysicsWorld.h", "private:\n", "    void Stage0DropContactWarmCache();\nprivate:\n")
        with (source / "src/physics/WorldPersistence.cpp").open("a",encoding="utf-8") as f:
            f.write("\n// Diagnostic ablation only; never used by the candidate path.\nvoid PhysicsWorld::Stage0DropContactWarmCache(){m_impl->contactCache.clear();}\n")
    physics = re.search(r"set\(JUDAS_PHYSICS_SOURCES\s+(.*?)\n\)",
                        (root / "CMakeLists.txt").read_text(), re.S).group(1).split()
    world = re.search(r"set\(JUDAS_PHYSICS_WORLD_SOURCES\s+(.*?)\n\)",
                      (root / "cmake/PhysicsWorldSources.cmake").read_text(), re.S).group(1).split()
    physics += ["src/JobSystem.cpp", "src/PerformanceProfiler.cpp"]
    cmake = """cmake_minimum_required(VERSION 3.20)
project(JudasPlanarStage0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)
add_library(native_physics STATIC
""" + "\n".join(physics + world) + """
)
target_include_directories(native_physics PUBLIC src PRIVATE third_party/quickhull)
target_link_libraries(native_physics PUBLIC Threads::Threads)
target_compile_options(native_physics PRIVATE -Wall -Wextra -Wpedantic)
add_executable(native_rows NativeRows.cpp)
target_link_libraries(native_rows PRIVATE native_physics)
add_executable(trajectory_3d Control3D.cpp)
target_link_libraries(trajectory_3d PRIVATE native_physics)
add_executable(existing_mk2 tests/Mk2PhysicsTests.cpp)
target_link_libraries(existing_mk2 PRIVATE native_physics)
add_executable(existing_rotation tests/Mk2RotationTests.cpp)
target_link_libraries(existing_rotation PRIVATE native_physics)
"""
    if args.baseline:
        cmake=cmake.replace("add_executable(native_rows NativeRows.cpp)\ntarget_link_libraries(native_rows PRIVATE native_physics)\n", "")
    if args.reduced:
        cmake += "target_compile_definitions(native_rows PRIVATE PLANAR_REDUCED)\n"
        cmake += """add_executable(legacy_hinge NativeRows.cpp LegacyJoints.cpp)
target_include_directories(legacy_hinge PRIVATE src/physics)
target_compile_definitions(legacy_hinge PRIVATE PLANAR_REDUCED)
target_link_libraries(legacy_hinge PRIVATE native_physics)
"""
    (source / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
    # Regenerating the same prototype must not force a redundant native build.
    for path, (old_hash, old_time) in before.items():
        if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest() == old_hash:
            os.utime(path, ns=(old_time, old_time))
    paths = [root / p for p in physics + world]
    paths += list((root / "src").rglob("*.h")) + list((root / "src").rglob("*.inl"))
    paths += list((root / "third_party/quickhull").rglob("*.hpp"))
    paths += [root/"tests/Mk2PhysicsTests.cpp",root/"tests/Mk2RotationTests.cpp"]
    manifest = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(paths))}
    (output / "BASE_SOURCES.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    if args.prepare_only:
        return
    build = output / "build-make"
    subprocess.run(["cmake", "-S", str(source), "-B", str(build), "-G", "Unix Makefiles",
                    "-DCMAKE_BUILD_TYPE=Release"], check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], check=True)
    if not args.build_only:
        subprocess.run([str(build / ("trajectory_3d" if args.baseline else "native_rows"))], check=True)


if __name__ == "__main__":
    main()
