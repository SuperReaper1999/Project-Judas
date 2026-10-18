#!/usr/bin/env python3
"""Lossless owned Pass5 evidence publication, excluding executables/archives."""
from pathlib import Path
import datetime,gzip,hashlib,json,shutil
r=Path.cwd();cache=r/'.cache/prealpha-pass5';dense=cache/'dense';out=r/'docs/evidence/prealpha-pass5/dense'
out.mkdir(parents=True,exist_ok=True);encoding=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def copy(origin,dest):
    dest.parent.mkdir(parents=True,exist_ok=True)
    if origin.stat().st_size>100000 and origin.suffix in ['.json','.jsonl'] and ('profile' in origin.name or origin.suffix=='.jsonl'):
        data=origin.read_bytes();target=dest.with_suffix(dest.suffix+'.gz');target.write_bytes(gzip.compress(data,mtime=0))
        encoding.append(dict(path=target.relative_to(out).as_posix(),uncompressedBytes=len(data),uncompressedSha256=hashlib.sha256(data).hexdigest(),source=str(origin.relative_to(r))))
    else:shutil.copyfile(origin,dest)
for folder,label in [(cache/'baseline-run','baseline'),(dense/'observer-run','instrumented-memo-probe'),(dense/'fat-witness-run','candidate-witness'),(dense/'fat-cache-run','preliminary-candidate'),*( (dense/n,n)for n in ['confirm-A2','confirm-B2','confirm-A3','confirm-B3']if(dense/n).exists())]:
    for p in sorted(folder.rglob('*')):
        if p.is_file():copy(p,out/'runs'/label/p.relative_to(folder))
for name in ['FROZEN.json','judas-build-info-Release.json']:
    copy(cache/'baseline'/name,out/'baseline'/name)
copy(dense/'baseline/FROZEN_INPUTS.json',out/'baseline/PRECONFIGURE_INPUTS.json')
for name in ['observer-events.jsonl','candidate-witness.jsonl','CANDIDATE_WITNESS.json','HELPER_TESTS.json','CONFIRMATION_COMMANDS.json','ANALYSIS.json','ADOPTION_PROOF.json','helper-positive.log','helper-missing-revision-negative.log','helper-missing-filter-negative.log']:
    copy(dense/name,out/'observations'/name)
for folder in ['observer','fat-witness','fat-cache']:
    for p in sorted((dense/folder).iterdir()):
        if p.suffix in ['.json','.log']:copy(p,out/'builds'/folder/p.name)
for p in sorted((dense/'independent-review').iterdir()):
    if p.is_file():copy(p,out/'independent-review'/p.name)
for name in ['WorldImpactEvents.observer.cpp','WorldImpactEvents.fat-cache.cpp','WorldImpactEvents.fat-witness.cpp','ImpactCandidateCache.h','ImpactCandidateCacheTests.cpp','ImpactCandidateCache.omit-filter.h','ImpactCandidateCacheTests.omit-filter.cpp','build_observer.py','build_fat_cache.py','build_fat_witness.py','package_evidence.py','verify_adoption.py']:
    copy(dense/name,out/'reproduction'/name)
copy(r/'docs/evidence/prealpha-pass4/dense/reproduction/run_workloads.py',out/'reproduction/run_workloads.py')
(out/'COMPRESSED_INPUTS.json').write_text(json.dumps(dict(schema=1,scope='Lossless gzip with original raw-byte hashes; no source raw files removed.',files=encoding),indent=2)+'\n')
(out/'.gitattributes').write_text('* -text\n')
print('Published scoped evidence; report/manifest finalized after confirmation',len(encoding),'lossless compressed files')
