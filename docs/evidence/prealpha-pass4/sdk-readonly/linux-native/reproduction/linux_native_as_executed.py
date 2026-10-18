#!/usr/bin/env python3
"""Actual packaged SDK inspector: original failure, corrected read-only execution."""
from pathlib import Path
import datetime, hashlib, json, os, shutil, subprocess, sys, time

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'.cache/prealpha-pass4/sdk-readonly/linux-native'
EV=ROOT/'docs/evidence/prealpha-pass4/sdk-readonly/linux-native'
SDK=ROOT/'build/linux-sdk'
OUT.mkdir(parents=True,exist_ok=True);EV.mkdir(parents=True,exist_ok=True)
ENV=os.environ.copy()
# Exercise the ordinary user invocation; no -B or environment suppression.
for key in ['PYTHONDONTWRITEBYTECODE','PYTHONPYCACHEPREFIX']:
    ENV.pop(key,None)

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(path,value):path.write_text(json.dumps(value,indent=2)+'\n')
def inventory(folder):
    return {str(p.relative_to(folder)):dict(bytes=p.stat().st_size,sha256=sha(p))for p in sorted(folder.rglob('*'))if p.is_file()}
def run(name,args,expected):
    path=EV/(name+'.log');start=time.monotonic()
    with path.open('w')as log:r=subprocess.run([str(a)for a in args],cwd=ROOT,env=ENV,stdout=log,stderr=subprocess.STDOUT,timeout=90)
    row=dict(name=name,argv=[str(a)for a in args],cwd=str(ROOT),utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
             environmentRemoved=['PYTHONDONTWRITEBYTECODE','PYTHONPYCACHEPREFIX'],exit=r.returncode,expectedExit=expected,
             passed=r.returncode==expected,seconds=time.monotonic()-start,logSha256=sha(path))
    rows=json.loads((EV/'RESULTS.json').read_text())if(EV/'RESULTS.json').exists()else[];rows.append(row);save(EV/'RESULTS.json',rows)
    print(name,r.returncode,round(row['seconds'],3),flush=True);assert row['passed'],name

before=inventory(SDK);assert not list(SDK.rglob('__pycache__'))
old=OUT/'old-sdk';assert not old.exists();shutil.copytree(SDK,old,copy_function=os.link)
assert inventory(old)==before
run('old_verify_before',[sys.executable,ROOT/'tools/package_sdk.py','verify',old],0)
run('old_packaged_inspector',[sys.executable,old/'tools/inspect_sdk.py',old,'--host-check','--output',EV/'old-qualification.json'],1)
generated={p:v for p,v in inventory(old).items()if p not in before}
assert generated and all('__pycache__' in p and p.endswith('.pyc')for p in generated),generated
run('old_verify_after',[sys.executable,ROOT/'tools/package_sdk.py','verify',old],1)
assert inventory(SDK)==before,'Original SDK was mutated'
save(EV/'ORIGINAL_FAILURE.json',dict(originalSdkManifestSha256=sha(SDK/'SDK_MANIFEST.json'),
    originalFiles=len(before),generatedFiles=generated,originalSdkUnchanged=True,qualification=json.loads((EV/'old-qualification.json').read_text())))
shutil.copyfile(old/'tools/inspect_sdk.py',EV/'reproduction/original_packaged_inspect_sdk.py')

corrected=OUT/'corrected-sdk';assert not corrected.exists()
tools={name:sha(ROOT/'tools'/name)for name in ['inspect_sdk.py','verify_android.py']}
run('stage_corrected_sdk',[sys.executable,ROOT/'tools/package_sdk.py','stage','--source',ROOT,'--bin-dir',ROOT/'build',
    '--build-info',ROOT/'build/judas-build-info-Release.json','--output',corrected,'--probe-tools'],0)
published=inventory(corrected)
run('corrected_verify_before',[sys.executable,corrected/'tools/package_sdk.py','verify',corrected],0)
run('corrected_packaged_inspector',[sys.executable,corrected/'tools/inspect_sdk.py',corrected,'--host-check','--output',EV/'corrected-qualification.json'],0)
run('corrected_packaged_android_help',[sys.executable,corrected/'tools/verify_android.py','--help'],0)
run('corrected_verify_after',[sys.executable,corrected/'tools/package_sdk.py','verify',corrected],0)
assert inventory(corrected)==published and not list(corrected.rglob('__pycache__'))
assert {name:sha(ROOT/'tools'/name)for name in tools}==tools,'Correction changed during qualification'

# Unrelated unexpected bytecode remains an error; the verifier is not weakened.
tampered=OUT/'tampered-sdk';assert not tampered.exists();shutil.copytree(corrected,tampered,copy_function=os.link)
unexpected=tampered/'tools/__pycache__/unexpected-control.pyc';unexpected.parent.mkdir();unexpected.write_bytes(b'Untracked bytecode must remain rejected.\n')
run('unexpected_pyc_negative',[sys.executable,tampered/'tools/package_sdk.py','verify',tampered],1)
assert inventory(corrected)==published and inventory(SDK)==before
save(EV/'QUALIFICATION.json',dict(sourceHead=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
    changedToolSha256=tools,nativeBuild=json.loads((SDK/'BUILD_INFO.json').read_text()),
    correctedSdkManifestSha256=sha(corrected/'SDK_MANIFEST.json'),toolCount=len(json.loads((corrected/'SDK_MANIFEST.json').read_text())['tools']),
    originalSdkUnchanged=True,correctedSdkUnchanged=True,noCacheDirectories=True,unexpectedPycRejected=True,
    compiledBinariesUnchanged=all(before[n]==published[n]for n in json.loads((corrected/'SDK_MANIFEST.json').read_text())['tools']),
    scope='Real pre-existing17Linux Release binaries, corrected packaged Python entries. No C++ build, no rewritten provenance, no -B or bytecode-suppression environment.'))
shutil.copyfile(corrected/'SDK_MANIFEST.json',EV/'CORRECTED_SDK_MANIFEST.json')
print('Native packaged SDK before/after qualification complete',flush=True)
