#!/usr/bin/env python3
"""Minimal requalification after byte-equivalent MSVC string-literal splitting."""
from pathlib import Path
import datetime, hashlib, importlib.util, json, shutil, subprocess

ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('qualification',ROOT/'.cache/prealpha-pass4/linux/run_final.py')
q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
q.OUT=ROOT/'.cache/prealpha-pass4/linux-final-portability'
q.EV=ROOT/'docs/evidence/prealpha-pass4/linux-final-portability'
q.OUT.mkdir(parents=True,exist_ok=True);q.EV.mkdir(parents=True,exist_ok=True)
q.ENV.update(XDG_DATA_HOME=str(q.OUT/'user-data'),XDG_CONFIG_HOME=str(q.OUT/'config'))
sdk=ROOT/'build/linux-sdk'
source=ROOT/'src/ScriptSystem.cpp';source_hash=q.digest(source)
q.save(q.EV/'SOURCE_FINGERPRINTS.json',dict(baseCheckpoint=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
    files={'src/ScriptSystem.cpp':source_hash,'tests/ScriptTests.cpp':q.digest(ROOT/'tests/ScriptTests.cpp'),
           'tests/JudasJSExamplesTests.cpp':q.digest(ROOT/'tests/JudasJSExamplesTests.cpp')},
    rationale='Adjacent C++ raw string literals have an identical concatenated JS payload. No numerical/physics change; no second performance run.'))
assert q.run('build_literal_portability',['cmake','--build',ROOT/'build','--target','judas_linux_sdk','judas_script_tests','judasjs_examples_tests','-j2'],timeout=600)
assert q.digest(source)==source_hash
shutil.copyfile(ROOT/'build/judas-build-info-Release.json',q.EV/'BUILD_INFO.json')
q.run('script_runtime',[ROOT/'build/judas_script_tests'])
q.run('gravity_selection_cookbook',[ROOT/'build/judasjs_examples_tests',q.OUT/'cookbook','gravity-selection'])
q.run('live_surface_cookbook',[ROOT/'build/judasjs_examples_tests',q.OUT/'cookbook','surface'])
q.run('api_types_live_drift',['node',ROOT/'scripts/check_judasjs_api.mjs','--typescript',ROOT/'tools/verification/node_modules/typescript/lib/typescript.js','--runtime',q.OUT/'cookbook/surface.json'])
q.run('sdk_final_verify',['python3',ROOT/'tools/package_sdk.py','verify',sdk])
q.run('sdk_final_native_loader',['python3',ROOT/'tools/inspect_sdk.py',sdk,'--host-check','--output',q.EV/'SDK_QUALIFICATION.json'])
project=ROOT/'.cache/prealpha-pass4/linux/stress-project/physics_stress_lab.judasproj'
q.run('sdk_final_export',[sdk/'judas_export',project,q.OUT/'stress-export',sdk/'judas'],timeout=180)
moved=Path('/tmp/Judas-PreAlphaPass4-Physics-portability');assert not moved.exists();shutil.copytree(q.OUT/'stress-export',moved)
script=q.OUT/'relocated.txt';script.write_text('FRAMES 90\nLOG_EVERY 0\nACTION watch 1 1 2\nSCREENSHOT 60 '+str(q.OUT/'relocated.png')+'\n')
q.run('sdk_final_relocated',[moved/'judas'],cwd=Path('/tmp'),extra={'JUDAS_TEST_SCRIPT':str(script)},timeout=180)
for p in [sdk/'SDK_MANIFEST.json',q.OUT/'stress-export/BUILD_PROVENANCE.json',q.OUT/'stress-export/DEPENDENCIES.json',q.OUT/'relocated.png',q.OUT/'cookbook/gravity-selection.json',q.OUT/'cookbook/surface.json']:
    shutil.copyfile(p,q.EV/p.name)
tools=json.loads((ROOT/'packaging/sdk-inventory.json').read_text())['tools']
q.save(q.EV/'BINARY_FINGERPRINTS.json',dict(archiveSha256=q.digest(ROOT/'build/libjudas_engine.a'),
    binaries={p:q.digest(ROOT/'build'/p)for p in tools+['judas_script_tests','judasjs_examples_tests']}))
records=json.loads((q.EV/'RESULTS.json').read_text());assert all(x['exitMatchesExpectation']for x in records)
print('Final literal-portability followup:',len(records),'commands all matched expected exits',flush=True)
