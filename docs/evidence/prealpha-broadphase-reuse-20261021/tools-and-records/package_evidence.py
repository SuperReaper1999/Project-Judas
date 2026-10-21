#!/usr/bin/env python3
"""Plan source/data evidence only; copy after final freeze with --execute.

No builds, tests, benchmarks or Git mutations. Existing evidence is never edited.
Run inventories and pass/fail receipts are preserved, not reinterpreted by labels.
"""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
DEFAULT_OUTPUT=ROOT/'docs/evidence/prealpha-broadphase-reuse-20261021'
GROUPS=('baseline900','baseline1800','baseline-normal-live35','baseline-sampled-live35','stages900',
        'prediction900','prediction-live35','candidate-diagnostic900',
        'empty900','pose900','ordinary-prototypes','variants','guard-diagnostic',
        'sampled-build','sampled-build-v2','stage-build','candidate-diagnostic-build',
        'normal-live-comparison-v1','final900','final1800','final-live35',
        'final-analysis-short-long','final-analysis','final-stage-build','final-stages900',
        'qualifications','qualifications-final','cold','integration','app-drivers',
        'guard-original34','guard-broken34','helper-snapshots')
FUTURE=('final900','final1800','final-live35','qualifications-final')
SUFFIXES={'.json','.jsonl','.csv','.txt','.md','.log','.py','.h','.hpp','.cpp','.c','.cc',
          '.inl','.inc','.cmake','.make','.in','.js','.judas','.judasproj','.judasprefab','.judasui','.judasmeta'}
SKIP_PARTS={'__pycache__','objects','userdata','cache','config','.git','project',
            'editor-project','editor-owned','exported'}
BASELINE_SOURCES=('src/PhysicsWorld.cpp','src/physics/WorldBounds.cpp','src/physics/WorldInternal.h',
    'src/physics/WorldProfiling.h','src/InteractivePlay.cpp','src/PerformanceProfiler.h',
    'src/PerformanceProfiler.cpp','src/BuildIdentity.h','src/BuildIdentity.cpp',
    'tests/PhysicsStressLiveBenchmark.cpp','projects/physics_stress_lab/Scenes/room1024.judas',
    'projects/physics_stress_lab/Assets/scripts/lab.js','projects/physics_stress_lab/Assets/scripts/math.js')
CURRENT_SOURCES=('src/PhysicsWorld.cpp','tests/CandidateReuseTests.cpp',
                 'CMakeLists.txt','tools/validation/suites.json')
NOTES="""# Broadphase candidate-reuse follow-up evidence

Baseline: accepted `80f7b2a5f96cdf1d56c5c88043c16edefe70a4c3`.
This package contains original source-only prototypes, raw records and selected
current/frozen source snapshots. Folder names never certify test status.

Normal profile-off, sampler-disabled 900/1800/live35 runs are the direct timing
reference. CPU-PC samples, existing-profiler frame capture and instrumented
candidate-demand runs are diagnostic: preserve them separately and do not use
their timing as the normal performance comparison. Sampling reports direct main
thread software CPU-PC occupancy, not hardware cycles, stacks or worker/GPU CPU.

Raw profiler inclusive scopes overlap. Worker queue waits can begin in older
frames and finish here; they are not CPU cost. Existing stage captures preserve
thread IDs, parent/depth, origin/publication frames, fixed boundaries and all
drop/incomplete diagnostics. Snapshots are read after publication, never by
prematurely ending an engine frame. Main/worker elapsed spans are not additive.

The retained production change is only guarded reuse of the already-discovered
ordinary candidate set when wake-up/integration cannot have extended coverage.
The unconditional-reuse negative control, rejected empty-manifold and prepared-
pose experiments remain exact source/evidence, not production changes or claimed
additional speedups. Check final source/test inventories for the actual candidate.

Frame intervals belong to the preceding frame's work. Normal table tools match
frame IDs to every actual fixed step, compare full common-prefix physical/work
fields, and retain strict 0–10s,10–20s,20s–stop windows. Ordinary observation
timestamps do not provide exact fixed-step boundary clipping across wall windows.
Synthetic cadence is throughput, never displayed-FPS evidence. Live endpoints
after different numbers of steps must not be called a matched physical endpoint.

The frozen core exactly matched the prior accepted final binary inventory.
Runtime and normal benchmark were subsequently relinked with clean80f build
identity; both earlier and current hashes remain recorded rather than relabelled
byte-identical. The initial private sampler builder's wrong glad include failure
is retained; independent v2/stage build outputs retain their successful receipts.

CSV/JSONL files and JSON/log records larger than64KiB are losslessly stored as
deterministic gzip(mtime0,no filename).
The manifest records original/stored size+SHA256; all other records are exact byte
copies. Binaries, objects, static/shared libraries, source archives and temporary
assets are excluded. Their byte identities remain in original run/build receipts.
Only selected relevant source files are copied; baseline FROZEN.json retains the
complete516-source input inventory. Existing historical reports/evidence are not
modified. Missing final roots remain explicitly recorded, never silently passed.
"""


def sha(data):return hashlib.sha256(data).hexdigest()


def selected(path,relative):
    if any(part in SKIP_PARTS for part in relative.parts):return False
    if path.name.endswith('.original'):return path.name[:-9].endswith(('.cpp','.h'))
    return path.suffix in SUFFIXES or path.name in ('CMakeLists.txt','.clang-format')


def compressed(data):
    result=io.BytesIO()
    with gzip.GzipFile(fileobj=result,mode='wb',filename='',mtime=0,compresslevel=9) as stream:stream.write(data)
    return result.getvalue()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=DEFAULT_OUTPUT)
    p.add_argument('--map',action='append',default=[],metavar='LABEL=PATH')
    p.add_argument('--source',action='append',default=[],metavar='REPOSITORY_PATH')
    p.add_argument('--require',action='append',default=[],metavar='LABEL')
    p.add_argument('--max-file-mib',type=int,default=64)
    p.add_argument('--max-total-mib',type=int,default=256)
    p.add_argument('--execute',action='store_true')
    a=p.parse_args();output=a.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT/'docs/evidence'):
        raise SystemExit('Fresh docs/evidence destination required; no overwrite')
    if a.max_file_mib<=0 or a.max_total_mib<=0:raise SystemExit('Positive bounded size limits required')
    groups={label:HERE/label for label in GROUPS}
    for value in a.map:
        label,sep,path=value.partition('=')
        if not sep or not label or not all(c.isalnum() or c in '-_' for c in label):raise SystemExit('Safe LABEL=PATH required')
        path=Path(path).resolve()
        if not path.is_relative_to(ROOT/'.cache'):raise SystemExit('Mapped roots must be in this repository .cache')
        groups[label]=path
    for label in a.require:
        if label not in groups or not groups[label].is_dir():raise SystemExit('Required group unavailable: '+label)
    plans=[];destinations=set();skipped=[];missing=[];total=0
    def add(source,target):
        nonlocal total
        if source.is_symlink() or not source.is_file():raise RuntimeError('Regular input required: '+str(source))
        if source.stat().st_size>a.max_file_mib*1024**2:raise RuntimeError('Oversized input: '+str(source))
        raw=source.read_bytes();total+=len(raw)
        if total>a.max_total_mib*1024**2:raise RuntimeError('Total package bound exceeded')
        zipped=(target.suffix in ('.csv','.jsonl') or
                (target.suffix in ('.json','.log') and len(raw)>65536))
        stored=compressed(raw) if zipped else raw
        if zipped:target=target.with_suffix(target.suffix+'.gz')
        name=target.as_posix()
        if target.is_absolute() or '..' in target.parts or name in destinations:raise RuntimeError('Unsafe/colliding target: '+name)
        destinations.add(name)
        plans.append(({'path':name,'source':str(source),'original_bytes':len(raw),'original_sha256':sha(raw),
                       'stored_bytes':len(stored),'stored_sha256':sha(stored),
                       'encoding':'gzip-mtime0-no-filename' if zipped else 'exact-bytes'},stored))
    for label,folder in groups.items():
        if not folder.is_dir():missing.append({'label':label,'source':str(folder)});continue
        for source in sorted(folder.rglob('*')):
            if source.is_dir():continue
            relative=source.relative_to(folder)
            if source.is_symlink() or not selected(source,relative):
                skipped.append({'source':str(source),'reason':'not selected regular source/data format'});continue
            add(source,Path(label)/relative)
    # Root helper source/records are exact. No recursive catch-all scan includes
    # full frozen sources, binaries or any directory outside the selected roots.
    for source in sorted(HERE.iterdir()):
        if source.is_file() and not source.is_symlink() and selected(source,Path(source.name)):
            add(source,Path('tools-and-records')/source.name)
    baseline=HERE/'baseline'
    baseline_inputs=[baseline/'FROZEN.json',baseline/'judas-build-info-Release.json']
    baseline_inputs+=sorted((baseline/'recipes').glob('*'))+sorted((baseline/'prior-final-records').glob('*.json'))
    for source in baseline_inputs:
        if source.is_file() and not source.is_symlink():add(source,Path('baseline')/source.relative_to(baseline))
        else:missing.append({'label':'baseline-record','source':str(source)})
    for name in BASELINE_SOURCES:
        source=baseline/'source'/name
        if source.is_file():add(source,Path('baseline-selected-source')/name)
        else:missing.append({'label':'baseline-source','source':str(source)})
    for name in dict.fromkeys((*CURRENT_SOURCES,*a.source)):
        source=ROOT/name
        if not source.resolve().is_relative_to(ROOT) or source.is_symlink():raise RuntimeError('Unsafe current source: '+name)
        if source.is_file():add(source,Path('current-selected-source')/source.relative_to(ROOT))
        else:missing.append({'label':'current-source','source':name})
    report={'schema':'judas.broadphase-reuse-evidence.v1','execute':a.execute,'destination':str(output),
        'groups':{label:str(path) for label,path in groups.items()},
        'pending_final_groups_not_mapped':[label for label in FUTURE if label not in groups],
        'missing':missing,'skipped':skipped,'file_count':len(plans),'original_total_bytes':total,
        'stored_total_bytes':sum(row['stored_bytes'] for row,data in plans),'files':[row for row,data in plans],
        'status_policy':'Original receipts retain their status; directory names/presence are not inferred passes.'}
    if not a.execute:print(json.dumps(report,indent=2));return
    # Read/bound/hash/collision-check the entire inventory before any copying.
    output.mkdir(parents=True,exist_ok=False)
    for row,data in plans:
        target=output/row['path'];target.parent.mkdir(parents=True,exist_ok=True)
        with target.open('xb') as stream:stream.write(data)
    (output/'PROVENANCE.md').write_text(NOTES)
    (output/'PACKAGE.json').write_text(json.dumps(report,indent=2)+'\n')
    hashes=[sha(path.read_bytes())+'  '+path.relative_to(output).as_posix()
            for path in sorted(output.rglob('*')) if path.is_file()]
    (output/'SHA256SUMS').write_text('\n'.join(hashes)+'\n')
    print(json.dumps({key:value for key,value in report.items() if key not in ('files','skipped')},indent=2))


if __name__=='__main__':main()
