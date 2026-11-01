from pathlib import Path
import subprocess,hashlib,json,shutil,tarfile,datetime,platform
root=Path.cwd(); cache=root/'.cache/ai-usability-20261101'; dest=root/'docs/evidence/ai-usability-20261101'
assert not dest.exists()
added='''src/AIReservations.cpp
src/AIReservations.h
src/ScriptAIReservations.cpp
src/ScriptAIReservations.h
src/AISensing.cpp
src/AISensing.h
src/ScriptAISensing.cpp
src/ScriptAISensing.h
src/editor/EditorAIWorkflow.cpp
src/editor/EditorAIWorkflow.h
src/editor/AIBehaviourTemplate.h
tests/AIReservationTests.cpp
tests/AIReservationScriptTests.cpp
tests/AISensingTests.cpp
tests/AISensingScriptTests.cpp
tests/UtilityAIWorkflowTests.cpp
docs/AI_USABILITY_QUALIFICATION.md'''.splitlines()
modified=subprocess.check_output(['git','diff','--name-only'],text=True).splitlines()
assert len(modified)==22,modified
files=sorted(set(modified+added)); (cache/'TASK_FILES.json').write_text(json.dumps(files,indent=2)+'\n')
def sha(path):return hashlib.file_digest(path.open('rb'),'sha256').hexdigest()
def digest(paths):return {str(p.relative_to(root)):sha(p) for p in paths}
dest.mkdir(parents=True); (dest/'raw').mkdir()
for p in cache.iterdir():
    if p.is_file() and p.suffix in ('.log','.txt','.json','.csv','.png'):
        shutil.copy2(p,dest/'raw'/p.name)
shutil.copy2(cache/'assemble_evidence.py',dest/'assemble_evidence.py')
# The exact fresh project and the original cold participant bytes; no binaries.
with tarfile.open(dest/'fixtures.tar.gz','w:gz') as archive:
    for name in ('FreshAI-final','reservation-cold-qualified'):
        for p in sorted((cache/name).rglob('*')):
            if p.is_file():archive.add(p,arcname=str(Path(name)/p.relative_to(cache/name)))
    archive.add(cache/'little-haven-final/expected.json',arcname='little-haven-expected.json')
build=root/'.cache/2d-stage1-build'
names=['judas','judas_editor','judas_export','judas_ai_reservation_tests','judas_ai_reservation_script_tests','judas_ai_sensing_tests','judas_ai_sensing_script_tests','judas_ai_workflow_tests','judas_utility_ai_application_tests']
binaries=[build/n for n in names]
shutil.copy2(build/'judas-build-info-Release.json',dest/'TESTED_BUILD_PROVENANCE.json')
package=cache/'FreshAI-standalone'
shutil.copy2(package/'BUILD_PROVENANCE.json',dest/'PACKAGE_PROVENANCE.json')
checks={}
for name in ('src/Project.cpp','src/FluidWorld.cpp','src/PhysicsWorld.cpp','engine-version.json','projects/ai_workshop/Scenes/main.judas'):
    p=root/name
    if p.exists():
        original=subprocess.run(['git','show','HEAD:'+name],capture_output=True)
        if original.returncode==0:checks[name]={'sha256':sha(p),'unchanged_from_base':hashlib.sha256(original.stdout).hexdigest()==sha(p)}
assert all(x['unchanged_from_base'] for x in checks.values())
manifest={
 'format':1,'captured_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'base':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
 'main_unchanged':subprocess.check_output(['git','rev-parse','main'],text=True).strip(),
 'platform':platform.platform(),'machine':platform.machine(),
 'scope':'Automated native Linux qualification; no new human or other-platform acceptance',
 'changed_files':files,'file_sha256':digest([root/p for p in files]),
 'tested_binary_sha256':digest(binaries),
 'relocated_package_sha256':{str(p.relative_to(package)):sha(p) for p in sorted(package.rglob('*')) if p.is_file()},
 'unchanged_controls':checks,
 'provenance_note':'Binaries built from dirty candidate based on base commit before documentation/evidence checkpoint. See exact file and binary SHA-256 records; not claimed to embed the later checkpoint hash.'}
(dest/'FINGERPRINTS.json').write_text(json.dumps(manifest,indent=2)+'\n')
readme='''AI usability, optional sensing and target leases — 1 November 2026

Scope: Experimental continuation of 0bfb3a433d73362ee12e66fd37e667a36ed45ea2.
Main remains 49c4f3d9e29debe669411f01501e0276dea23231. No release or fluid unlock.
See ../../AI_USABILITY_QUALIFICATION.md for results, decisions and limits.

Authoritative successful runs
- raw/QUALIFIED_RESULTS.json: 16 focused suite invocations, all exit 0.
- raw/workflow-final-qualified.log: 49 fresh-object/authoring/real-motion checks.
- raw/reservation-cold-*-final.log and COLD_RESULTS.json: two separate processes.
- raw/LITTLE_HAVEN_RESULTS.json and little-haven-{write,cold}.log: native app,
  actual restored action/physical movement, 41+37 checks.
- raw/EDITOR_RESULTS.json/editor-play-stop.log: isolated preferences and project,
  edit, Play/Stop, save, identical authored scene. This is automated, not human UX acceptance.
- raw/EXPORT_RESULTS.json/standalone-relocated.log: whole package renamed/moved,
  cwd /tmp, JUDAS_ENGINE_ROOT unset, 1,200 frames. Final runtime/exporter match.
- raw/{api-final-check,manifest-final,registration-final}.log: API/type and gates.
- raw/sensing-benchmark.csv: 100 samples/arm, release GCC15.2/Ryzen5 5600.
  Cache hits are explicitly stale. maxCandidates does not bound broadphase work.

Preserved earlier failures/experiments
Earlier *first*, *verified*, workflow-second/final/trace, sensor-publication,
reload and export.log files are historical intermediate runs, not final results.
The report explains malformed entity references, placeholder preflight, early
sensor disposal, production-step scheduling and invalid test endpoint assumptions.
export.log preserves the mismatched-build provenance rejection. Final export is
export-qualified.log; no guard was bypassed. No failed fixture was deleted.

Reproduce from this checkpoint (new disposable output directories)
  cmake -S . -B /tmp/judas-ai-build -DCMAKE_BUILD_TYPE=Release
  cmake --build /tmp/judas-ai-build -j2 --target judas judas_editor judas_export judas_ai_reservation_tests judas_ai_reservation_script_tests judas_ai_sensing_tests judas_ai_sensing_script_tests judas_ai_workflow_tests judas_utility_ai_application_tests
  /tmp/judas-ai-build/judas_ai_reservation_tests
  /tmp/judas-ai-build/judas_ai_reservation_script_tests
  /tmp/judas-ai-build/judas_ai_sensing_tests
  /tmp/judas-ai-build/judas_ai_sensing_script_tests
  /tmp/judas-ai-build/judas_ai_workflow_tests /tmp/judas-ai-fresh
  /tmp/judas-ai-build/judas_ai_reservation_script_tests write /tmp/judas-ai-cold
  /tmp/judas-ai-build/judas_ai_reservation_script_tests read /tmp/judas-ai-cold
  /tmp/judas-ai-build/judas_utility_ai_application_tests projects/ai_workshop/AI_Workshop.judasproj /tmp/judas-ai-haven
  /tmp/judas-ai-build/judas_utility_ai_application_tests projects/ai_workshop/AI_Workshop.judasproj /tmp/judas-ai-haven cold
  /tmp/judas-ai-build/judas_ai_sensing_tests --benchmark
  node scripts/check_judasjs_api.mjs
  python3 scripts/prealpha_validate.py --check-manifest
  python3 -m unittest discover -s tests -p test_validation_registration.py

Actual desktop/editor/export runs require a working GL desktop or appropriate
qualified virtual display. Run graphics tests sequentially. Build/export a matched
runtime/exporter pair, never combine older artifacts with the new exporter:
  /tmp/judas-ai-build/judas_export /tmp/judas-ai-fresh/FreshAI.judasproj /tmp/judas-ai-export /tmp/judas-ai-build/judas
Move the entire package to a different directory; run judas there from unrelated
cwd with JUDAS_ENGINE_ROOT unset and JUDAS_TEST_SCRIPT pointing at a copy of
raw/fresh-app.txt with the screenshot path adjusted. That script runs 1,200 frames.
Editor smoke uses an isolated copy, XDG_DATA_HOME=<fresh-user-data>,
JUDAS_EDITOR_AUTOTEST=<output-prefix>, judas_editor <project>. Check its log for
IDENTICAL authored scene after Play/Stop and successful save. Do not overwrite
an existing project or the operator's preferences.

fixtures.tar.gz contains the exact generated project and reservation cold fixture.
It contains no runtime binaries or source-dependent build caches. The live usable
package is .cache/ai-usability-20261101/FreshAI-standalone; rebuild if unavailable.
FINGERPRINTS.json records changed source/doc files, tested binaries, complete live
package contents and unchanged controls. Build identities honestly retain the
base+dirty development identity. SHA256SUMS verifies this evidence bundle.
Windows/Android/browser execution, universal sensing cost and pathfinding are not
claimed. Original research, fluid implementation and unrelated experiments stay untouched.
'''
(dest/'README.txt').write_text(readme)
# Hash every evidence member except the hash list itself; verify all copied bytes.
paths=sorted(p for p in dest.rglob('*') if p.is_file())
lines=[sha(p)+'  '+str(p.relative_to(dest)) for p in paths]
(dest/'SHA256SUMS').write_text('\n'.join(lines)+'\n')
for line in lines:
    expected,relative=line.split('  ',1);assert sha(dest/relative)==expected
print('Changed files:',len(files),'Evidence files:',len(paths)+1,'Evidence bytes:',sum(p.stat().st_size for p in dest.rglob('*') if p.is_file()))
