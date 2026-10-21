#!/usr/bin/env python3
"""Private one-TU comparisons; never edits build inputs or production source."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('name')
    ap.add_argument('--source', type=Path)
    ap.add_argument('--execute', action='store_true')
    args = ap.parse_args()
    source = (args.source or HERE/'ordinary-prototypes'/args.name/'PhysicsWorld.cpp').resolve()
    out = HERE/'variants'/args.name
    if out.exists(): raise RuntimeError('Fresh output required')
    frozen = json.loads((HERE/'baseline/FROZEN.json').read_text())
    core = HERE/'baseline/libjudas_engine.a'
    if sha(core) != frozen['artifacts']['libjudas_engine.a']['sha256']:
        raise RuntimeError('Frozen core changed')
    members = subprocess.check_output(['ar','t',str(core)],text=True).splitlines()
    if members.count('PhysicsWorld.cpp.o') != 1: raise RuntimeError('Ambiguous archive member')
    flags = dict(line.split(' = ',1) for line in (HERE/'baseline/recipes/engine-flags.make').read_text().splitlines() if ' = ' in line)
    obj = out/'PhysicsWorld.cpp.o'
    archive = out/'libjudas_engine.a'
    compile_cmd = ['/usr/bin/c++',*shlex.split(flags['CXX_DEFINES']),'-I'+str(source.parent),*shlex.split(flags['CXX_INCLUDES']),*shlex.split(flags['CXX_FLAGS']),'-g1','-c',str(source),'-o',str(obj)]
    sampled = json.loads((HERE/'sampled-build-v2/BUILD.json').read_text())
    link = [str(archive) if token == str(core) else token for token in sampled['link']]
    link[link.index('-o')+1] = str(out/'live')
    modules = {p:v for p,v in sampled['modules'].items() if p != str(core)}
    report = {'source':str(source),'source_sha256':sha(source),'baseline_core_sha256':sha(core),'compile':compile_cmd,'link':link,'cwd':str(ROOT/'build'),'modules':modules,'execute':args.execute}
    if not args.execute:
        print(json.dumps(report,indent=2));return
    if not all(sha(Path(p))==v for p,v in modules.items()): raise RuntimeError('Link module drift')
    out.mkdir(parents=True)
    for label,cmd in [('compile',compile_cmd)]:
        with (out/(label+'.log')).open('w') as log:
            code = subprocess.run(cmd,cwd=ROOT/'build',stdout=log,stderr=subprocess.STDOUT).returncode
        report[label+'_exit'] = code
        (out/'BUILD.json').write_text(json.dumps(report,indent=2)+'\n')
        if code: raise SystemExit(code)
    shutil.copy2(core,archive)
    subprocess.run(['ar','r',str(archive),str(obj)],check=True)
    subprocess.run(['ranlib',str(archive)],check=True)
    with (out/'link.log').open('w') as log:
        code = subprocess.run(link,cwd=ROOT/'build',stdout=log,stderr=subprocess.STDOUT).returncode
    report.update(link_exit=code,core_sha256=sha(archive),modules_unchanged=all(sha(Path(p))==v for p,v in modules.items()),source_unchanged=sha(source)==report['source_sha256'])
    if not code: report['binary_sha256'] = sha(out/'live')
    (out/'BUILD.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k.endswith(('sha256','exit','unchanged'))},indent=2))
    if code or not report['modules_unchanged'] or not report['source_unchanged']: raise SystemExit(code or 1)

if __name__ == '__main__': main()
