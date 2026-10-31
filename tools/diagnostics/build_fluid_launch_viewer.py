#!/usr/bin/env python3
"""Build the diagnostic driver against an existing, matching native engine build.
No engine source/build configuration is changed. The driver has debug symbols;
the linked engine retains its qualified build configuration and numerical behaviour.
"""
import argparse
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, required=True)
parser.add_argument('--output-dir', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = args.build_dir.resolve()
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles/judas.dir/flags.make').read_text(encoding='utf-8').splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
obj = out / 'FluidLaunchViewer.o'
exe = out / 'fluid_launch_viewer'
subprocess.run(['c++', '-std=c++17', '-O2', '-g', '-Wall', '-Wextra',
                *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], '-c',
                str(root / 'tools/diagnostics/FluidLaunchViewer.cpp'), '-o', str(obj)], check=True)
link = shlex.split((build / 'CMakeFiles/judas.dir/link.txt').read_text(encoding='utf-8'))
# Retain the qualified target's exact static library ordering/dependencies.
libraries = link[link.index('-o') + 2:]
gate_obj = out / 'FluidDiagnosticForceGate.o'
subprocess.run(['c++', '-std=c++17', '-O2', '-g', '-Wall', '-Wextra',
                *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], '-c',
                str(root / 'tools/diagnostics/FluidDiagnosticForceGate.cpp'), '-o', str(gate_obj)], check=True)
symbols = [
    '_ZN23ProductionFluidCoupling16PrepareRigidStepER12RuntimeWorldf',
    '_ZN12PhysicsWorld10ApplyForceE10BodyHandleRKN3glm3vecILi3EfLNS1_9qualifierE0EEE',
    '_ZN12PhysicsWorld11ApplyTorqueE10BodyHandleRKN3glm3vecILi3EfLNS1_9qualifierE0EEEb',
    '_ZN16PlayerController14SetFluidSampleERK17PlayerFluidSample',
    '_ZN12PhysicsWorld21SolveParticleContactsERSt6vectorINS_15ContactParticleESaIS1_EERKS0_INS_23ParticleBoundaryContactESaIS5_EERS0_IN3glm3vecILi3EfLNSA_9qualifierE0EEESaISD_EEPSD_f',
]
wraps = ['-Wl,--wrap=' + name for name in symbols]
subprocess.run([link[0], str(obj), str(gate_obj), *wraps, '-o', str(exe), *libraries], cwd=build, check=True)
print(exe)
