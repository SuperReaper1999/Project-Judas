#!/usr/bin/env python3
"""Prepare one exact-source Windows matrix after root freezes the candidate.

This script writes only preparation inputs; it never compiles or runs the VM.
"""
import argparse,json,pathlib,subprocess,sys

p=argparse.ArgumentParser()
p.add_argument('label')
p.add_argument('--source-root',type=pathlib.Path,default=pathlib.Path.cwd())
a=p.parse_args();root=pathlib.Path.cwd()
plan=json.loads((root/'docs/evidence/m78-continuation/windows-final-matrix-plan.json').read_text())
args=[sys.executable,str(root/'docs/evidence/m78-continuation/probes/prepare_windows_direct_manifest.py'),a.label,'--stage-tests','--header-closure','--source-root',str(a.source_root)]
for source in ['src/RigidBody.cpp','src/PhysicsWorld.cpp','src/CollisionGeometry.cpp']:args+=['--replace',source]
for header in ['src/RigidBody.h','src/PhysicsWorld.h','src/CollisionGeometry.h','src/RigidMotion.h']:args+=['--header',header]
for case in plan['cases']:
 args+=['--case',case['id']+'='+case['source']]
 for source in case['extraSources']:args+=['--extra',case['id']+'='+source]
 if case['recipeTemplate']!='judas_balance_lab_tests':args+=['--case-template',case['id']+'='+case['recipeTemplate']]
subprocess.run(args,check=True)
path=root/'.cache/windows-vm-transfer'/f'm78-continuation-direct-{a.label}.json';manifest=json.loads(path.read_text())
output='C:\\JudasValidation\\M78ContinuationResults\\'+a.label
package='C:\\JudasValidation\\M78ContinuationPackage-'+a.label
runtime='C:\\JudasValidation\\Build\\Release\\m78_'+a.label+'_runtime.exe'
navProject='C:\\JudasValidation\\M78ContinuationNav-'+a.label+'\\landscape_fork.judasproj'
def resolve(text):
 for before,after in {'@OUT@':output,'@PACKAGE@':package,'@RUNTIME@':runtime,'@NAVPROJECT@':navProject}.items():text=text.replace(before,after)
 return text
byid={c['id']:c for c in plan['cases']}
for case in manifest['cases']:
 row=byid[case['id']];case['arguments']=[resolve(x) for x in row['arguments']]
 case['environment']={k:resolve(v) for k,v in row.get('requiresEnvironment',{}).items()}
 case['purpose']=row['purpose']
manifest['additionalRuns']={case:[{'id':r['id'],'arguments':[resolve(x) for x in r['arguments']]} for r in runs] for case,runs in plan['additionalRuns'].items()}
manifest['ownedOutput']=output;manifest['ownedPackage']=package;manifest['ownedNavigationFixtureProject']=navProject
manifest['minimumFreeBeforeLibraryBytes']=160*1024*1024
manifest['scope']='Fresh exact-source combined M78 native matrix; run only after root final freeze and resource clearance. Original M77 fixtures and thresholds unchanged.'
path.write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'scope':'PREPARED ONLY; no VM/build/test command launched','manifest':str(path),'cases':len(manifest['cases']),'ownedOutput':output}))
