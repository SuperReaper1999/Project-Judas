#!/usr/bin/env python3
"""File-only freeze of accepted 80f7 baseline. No builds or workloads.

Prints a plan by default; --execute copies only bounded, explicitly selected
qualified binaries, source inputs/recipes and fixture data into a fresh folder.
"""
from __future__ import annotations
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
ACCEPTED = '80f7b2a5f96cdf1d56c5c88043c16edefe70a4c3'
PRIOR = ROOT / 'docs/evidence/prealpha-support-continuity-20261021/final-records'


def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            value.update(chunk)
    return value.hexdigest()


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=HERE/'baseline')
    parser.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    destination = args.output.resolve()
    if not destination.is_relative_to(HERE) or destination.exists():
        raise SystemExit('Fresh destination under this private study required')
    head, status = git('rev-parse','HEAD'), git('status','--porcelain','--untracked-files=no')
    if head != ACCEPTED or status:
        raise SystemExit('Accepted clean tracked80f7 state required before baseline freeze')
    names = git('ls-files','--','src','cmake','CMakeLists.txt','tests/PhysicsStressLiveBenchmark.cpp',
                'projects/physics_stress_lab').splitlines()
    sources = {name:{'sha256':sha(ROOT/name),'bytes':(ROOT/name).stat().st_size}
               for name in names if (ROOT/name).is_file()}
    artifacts = {
        'libjudas_engine.a': ROOT/'build/libjudas_engine.a',
        'judas': ROOT/'build/judas',
        'judas_physics_stress_live_benchmark': ROOT/'build/judas_physics_stress_live_benchmark',
        'judas-build-info-Release.json': ROOT/'build/judas-build-info-Release.json',
        'recipes/benchmark-flags.make': ROOT/'build/CMakeFiles/judas_physics_stress_live_benchmark.dir/flags.make',
        'recipes/benchmark-build.make': ROOT/'build/CMakeFiles/judas_physics_stress_live_benchmark.dir/build.make',
        'recipes/benchmark-link.txt': ROOT/'build/CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt',
        'recipes/engine-flags.make': ROOT/'build/CMakeFiles/judas_engine.dir/flags.make',
        'recipes/engine-link.txt': ROOT/'build/CMakeFiles/judas_engine.dir/link.txt',
        'prior-final-records/CURRENT_BINARY_HASHES.json': PRIOR/'CURRENT_BINARY_HASHES.json',
        'prior-final-records/SOURCE_INPUTS.json': PRIOR/'SOURCE_INPUTS.json',
    }
    qualified = json.loads((PRIOR/'CURRENT_BINARY_HASHES.json').read_text())['files']
    for key in ('build/libjudas_engine.a',):
        if sha(ROOT/key) != qualified[key]:
            raise SystemExit('Current binary differs from qualified accepted bytes: '+key)
    binary_lineage = {key:{'current_sha256':sha(ROOT/key),'earlier_qualified_sha256':qualified[key],
                           'exact_bytes_match':sha(ROOT/key)==qualified[key]}
                      for key in ('build/libjudas_engine.a','build/judas','build/judas_physics_stress_live_benchmark')}
    info = json.loads((ROOT/'build/judas-build-info-Release.json').read_text())
    if info.get('source_revision') != ACCEPTED or info.get('source_dirty'):
        raise SystemExit('Current linked identity metadata does not describe accepted clean80f7')
    records = {name:{'source':str(path),'sha256':sha(path),'bytes':path.stat().st_size}
               for name,path in artifacts.items()}
    if any(row['bytes'] > 128*1024**2 for row in records.values()) or sum(row['bytes'] for row in records.values()) > 256*1024**2:
        raise SystemExit('Baseline artifact copy exceeds bounded lean inventory')
    fixture = 'projects/physics_stress_lab/Scenes/room1024.judas'
    report = {'schema':'judas.accepted80f7.file-only-baseline.v1','accepted_head':ACCEPTED,
              'head':head,'branch':git('branch','--show-current'),'tracked_status':status,
              'execute':args.execute,'destination':str(destination),'sources':sources,
              'artifacts':records,'fixture':{'source':fixture,**sources[fixture]},
              'binary_lineage':binary_lineage,
              'build_identity_metadata':info,
              'notes':['Core binary bytes match accepted checkpoint final record exactly.',
                       'Runtime/benchmark were subsequently relinked with clean accepted80f build identity; earlier as-run binary hashes are preserved separately, not relabelled identical.',
                       'Main/Experimental branch promotion does not alter these source/binary bytes.',
                       'No old f562 baseline core is copied or reused as this accepted80f7 baseline.',
                       'No build, benchmark or graphics validation runs in this freeze operation.']}
    if not args.execute:
        print(json.dumps(report,indent=2));return
    destination.mkdir(parents=True)
    for name,path in artifacts.items():
        target = destination/name; target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,target)
        if sha(target)!=records[name]['sha256'] or sha(path)!=records[name]['sha256']:
            raise RuntimeError('Artifact changed while freezing: '+str(path))
    # Only source text/fixtures are copied; the binary project font is already
    # represented by its hash and remains in the ordinary project asset tree.
    omitted=[]
    for name,row in sources.items():
        path=ROOT/name
        if path.suffix.lower() in {'.ttf','.png','.jpg','.blend','.wav','.ogg','.glb','.gltf'}:
            omitted.append(name);continue
        target=destination/'source'/name;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,target)
        if sha(target)!=row['sha256'] or sha(path)!=row['sha256']:
            raise RuntimeError('Source changed while freezing: '+name)
    report['source_binary_assets_hash_only']=omitted
    report['utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
    report['all_sources_unchanged_after']=all((ROOT/name).is_file() and sha(ROOT/name)==row['sha256'] for name,row in sources.items())
    report['head_unchanged_after']=git('rev-parse','HEAD')==head
    (destination/'FROZEN.json').write_text(json.dumps(report,indent=2)+'\n')
    if not report['all_sources_unchanged_after'] or not report['head_unchanged_after']:
        raise SystemExit('Baseline changed while freezing; preserve receipt, do not use it as a clean baseline')
    print(json.dumps({'destination':str(destination),'artifacts':len(records),'source_inputs':len(sources),
                      'artifact_bytes':sum(row['bytes'] for row in records.values())},indent=2))


if __name__=='__main__':main()
