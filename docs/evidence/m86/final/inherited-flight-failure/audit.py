#!/usr/bin/env python3
"""Read existing build dependencies only; never build or execute native code."""
import datetime
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess

ROOT = Path('/home/conner/Documents/GitHub/Project-Judas')
BASE = '7d1110ee323e3f7d6a7b5d84be338edf144546dc'
OUT = Path(__file__).resolve().parent
BUILD = ROOT / 'build'
TARGET = 'judas_atmospheric_flight_tests'
TARGETS = (TARGET, 'judas_profiler', 'judas_app_icon')

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

def sha(data):
    return hashlib.sha256(data).hexdigest()

def record(path):
    p = Path(path)
    return {'path': str(p), 'bytes': p.stat().st_size, 'sha256': sha(p.read_bytes())}

def baseline(path):
    return git('show', BASE + ':' + str(path))

OUT.mkdir(parents=True, exist_ok=True)
dependencies = set()
dependency_receipts = []
for target in TARGETS:
    directory = BUILD / 'CMakeFiles' / (target + '.dir')
    for receipt in sorted(directory.rglob('*.o.d')):
        text = receipt.read_text().replace('\\\n', ' ')
        paths = shlex.split(text.split(':', 1)[1])
        dependencies.update(Path(p).resolve() for p in paths)
        dependency_receipts.append(record(receipt))

local, generated, external = [], [], []
for p in sorted(dependencies):
    try:
        rel = p.relative_to(ROOT)
    except ValueError:
        external.append(str(p))
        continue
    rec = record(p)
    rec['path'] = str(rel)
    if str(rel).startswith('build/'):
        generated.append(rec)
        continue
    try:
        old = baseline(rel)
    except subprocess.CalledProcessError:
        rec.update(baseline_sha256=None, matches_baseline=False)
    else:
        rec.update(baseline_sha256=sha(old), matches_baseline=rec['sha256'] == sha(old))
    local.append(rec)

# Embedded icon has one generated dependency; validate it against its unchanged
# baseline template and baseline original PNG, rather than treating it as source.
template = baseline('cmake/JudasAppIcon.h.in').decode()
png = baseline('assets/branding/judas-icon.png')
expected_icon = template.replace('@JUDAS_APP_ICON_BYTES@', ''.join(f'0x{v:02x},' for v in png)).encode()
icon = BUILD / 'generated' / 'JudasAppIcon.h'
icon_check = {'path': str(icon), 'actual_sha256': sha(icon.read_bytes()),
              'baseline_generated_sha256': sha(expected_icon),
              'matches_baseline_generated': icon.read_bytes() == expected_icon}

current_cmake = (ROOT / 'CMakeLists.txt').read_text()
old_cmake = baseline('CMakeLists.txt').decode()
def target_block(text):
    start = text.index('add_executable(' + TARGET)
    end = text.index('judas_warnings(' + TARGET + ')', start) + len('judas_warnings(' + TARGET + ')')
    return text[start:end]
def physics_block(text):
    start = text.index('set(JUDAS_PHYSICS_SOURCES')
    return text[start:text.index('\n)', start) + 2]
cmake_checks = {
    'global_configuration_prefix_unchanged': current_cmake.split('add_library(judas_engine STATIC', 1)[0] == old_cmake.split('add_library(judas_engine STATIC', 1)[0],
    'target_definition_unchanged': target_block(current_cmake) == target_block(old_cmake),
    'physics_source_set_unchanged': physics_block(current_cmake) == physics_block(old_cmake),
}
link = (BUILD / 'CMakeFiles' / (TARGET + '.dir') / 'link.txt').read_text()
cmake_checks['no_judas_engine_or_weather_link'] = not any(v in link for v in ('libjudas_engine', 'Weather.cpp.o', 'WorldWeather.cpp.o', 'Renderer.cpp.o'))

evidence = OUT / 'captured'
evidence.mkdir(exist_ok=True)
for target in TARGETS:
    directory = BUILD / 'CMakeFiles' / (target + '.dir')
    for name in ('DependInfo.cmake', 'flags.make', 'link.txt'):
        source = directory / name
        if source.exists():
            shutil.copyfile(source, evidence / (target + '-' + name))
failure = Path('/tmp/judas-m86-final-headless') / (TARGET + '.log')
shutil.copyfile(failure, evidence / failure.name)
artifacts = [record(BUILD / TARGET), record(BUILD / 'libjudas_profiler.a'), record(BUILD / 'libjudas_app_icon.a')]
tracked_changes = set(git('diff', '--name-only', BASE).decode().splitlines())
intersection = sorted(tracked_changes.intersection(r['path'] for r in local))
result = {
    'audit_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'baseline': BASE,
    'head': git('rev-parse', 'HEAD').decode().strip(),
    'branch': git('branch', '--show-current').decode().strip(),
    'native_code_executed_or_built_by_this_audit': False,
    'dependency_receipt_count': len(dependency_receipts),
    'project_dependency_count': len(local),
    'unchanged_project_dependency_count': sum(r['matches_baseline'] for r in local),
    'changed_project_dependencies': [r for r in local if not r['matches_baseline']],
    'changed_tracked_file_intersection': intersection,
    'cmake_checks': cmake_checks,
    'generated_icon_check': icon_check,
    'project_dependencies': local,
    'generated_dependencies': generated,
    'external_dependencies': external,
    'dependency_receipts': dependency_receipts,
    'binary_artifacts': artifacts,
    'failure_log': record(failure),
    'conclusion': 'Source/dependency isolation establishes that no current M86 runtime, renderer, scene, save or weather source enters this separately compiled target. The recorded failure must remain reported; no separate baseline executable was built or run by this audit, so it is source-identical baseline isolation rather than an independent baseline execution.',
}
(OUT / 'dependency-hashes.json').write_text(json.dumps(result, indent=2) + '\n')
lines = [
    '# Atmospheric-flight regression: baseline isolation', '',
    f'Baseline and current HEAD: `{BASE}`; branch `Experimental`.',
    'This audit only read existing dependency receipts and source bytes. It did not build or run any native test.', '',
    f'- Existing failure: `manual torque builds a tumble that persists with SAS off in gas` (one assertion).',
    f'- {len(dependency_receipts)} compiled-unit dependency receipts, including linked profiler and app-icon static libraries.',
    f'- {len(local)} repository source/header dependencies; {result["unchanged_project_dependency_count"]} match baseline SHA-256 exactly.',
    f'- Changed-source intersection: {intersection}.',
    f'- CMake isolation checks: {json.dumps(cmake_checks, sort_keys=True)}.',
    f'- Generated embedded icon matches exact generation from baseline template/PNG: {icon_check["matches_baseline_generated"]}.',
    f'- Recorded failing binary SHA-256: `{artifacts[0]["sha256"]}`.', '',
    'The target directly compiles its existing flight, atmosphere, rigid-body and collision sources. It links only the existing profiler/app-icon libraries and SDL2, not `judas_engine` or any M86 weather/renderer/world implementation.', '',
    '**Classification:** inherited/source-isolated failure; do not report this target as passing. This evidence rules out an M86 source/dependency change in the target, but does not replace an independently run baseline executable or diagnose the original test contract.', '',
    'Exact dependency SHA-256 pairs and copied flags/link/receipt metadata are in `dependency-hashes.json` and `captured/`.', '',
    'Reproduce this read-only comparison: `python3 /tmp/m86-atmospheric-baseline-isolation/audit.py`.',
    'The existing failing test command was `build/judas_atmospheric_flight_tests`; this audit deliberately did not repeat it.',
]
(OUT / 'REPORT.md').write_text('\n'.join(lines) + '\n')
print(json.dumps({k: result[k] for k in ('dependency_receipt_count', 'project_dependency_count', 'unchanged_project_dependency_count', 'changed_project_dependencies', 'changed_tracked_file_intersection', 'cmake_checks', 'generated_icon_check')}, indent=2))
if result['changed_project_dependencies'] or intersection or not all(cmake_checks.values()) or not icon_check['matches_baseline_generated']:
    raise SystemExit(1)
