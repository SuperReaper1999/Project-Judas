import os,sys,subprocess,time,json,shutil,hashlib
from pathlib import Path
r=Path(__file__).resolve().parent;loc=json.loads((r/'LOCATIONS-corrected.json').read_text());sdk=Path(loc['sdk']);cwd=Path(loc['cwd']);root=Path(loc['root']);build=r/'build-linux';logs=r/'receipts/linux-corrected';logs.mkdir();rows=[]
shutil.copytree(build/'linux-sdk',sdk)
env={k:v for k,v in os.environ.items() if not k.startswith(('JUDAS_','CMAKE_','CCACHE_')) and k not in ['LD_LIBRARY_PATH','LIBRARY_PATH','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','PKG_CONFIG_PATH','PYTHONPATH']}
env.update(PATH='/usr/bin:/bin',XDG_DATA_HOME=str(root/'UserData'),XDG_CONFIG_HOME=str(root/'Config'),XDG_CACHE_HOME=str(root/'Cache'),PYTHONNOUSERSITE='1',SDL_AUDIODRIVER='dummy')
def run(name,args,working=cwd,expected=0,extra=None):
 e=env.copy();e.update(extra or {});start=time.monotonic()
 with (logs/(name+'.log')).open('w') as f:p=subprocess.run(list(map(str,args)),cwd=working,env=e,stdout=f,stderr=subprocess.STDOUT,timeout=240)
 rows.append({'name':name,'argv':list(map(str,args)),'cwd':str(working),'exit':p.returncode,'expectedExit':expected,'seconds':time.monotonic()-start});(logs/'LINUX_APPLICATIONS.json').write_text(json.dumps(rows,indent=2)+'\n');print(name,p.returncode,flush=True)
 if p.returncode!=expected:raise RuntimeError(name+' failed; original log preserved')
def hashes(folder):return {str(p.relative_to(folder)):hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.rglob('*') if p.is_file()}
before=hashes(sdk)
run('sdk-moved-verify',[sys.executable,sdk/'tools/package_sdk.py','verify',sdk])
run('sdk-host-loader',[sys.executable,sdk/'tools/inspect_sdk.py',sdk,'--host-check','--output',logs/'SDK_HOST.json'])
run('sdk-baseline-negative',[sys.executable,sdk/'tools/inspect_sdk.py',sdk,'--host-check','--max-glibc','2.39','--require-os','ubuntu:24.04','--output',logs/'SDK_BASELINE_NEGATIVE.json'],expected=1)
held=root/'held-judas-server';(sdk/'judas_server').rename(held)
try:run('sdk-missing-tool-negative',[sys.executable,sdk/'tools/package_sdk.py','verify',sdk],expected=1)
finally:held.rename(sdk/'judas_server')
run('sdk-restored-verify',[sys.executable,sdk/'tools/package_sdk.py','verify',sdk])
assert hashes(sdk)==before,'SDK verification/inspection mutated the payload'
for p in sdk.rglob('*'):
 if p.is_file():p.chmod(0o555 if os.access(p,os.X_OK) else 0o444)
for p in sorted((p for p in sdk.rglob('*') if p.is_dir()),key=lambda p:len(p.parts),reverse=True):p.chmod(0o555)
sdk.chmod(0o555)
run('native-identity',[build/'judas_build_identity_tests'])
run('native-storage',[build/'judas_windows_readiness_tests'])
run('first-project',[build/'judas_first_project_tests',root/'FirstProject'],working=sdk)
project=root/'FirstProject/My First Game/My_First_Game.judasproj'
run('first-project-script-check',[sdk/'judas_script_check',project])
run('first-project-export',[sdk/'judas_export',project,root/'Game Package résumé',sdk/'judas'])
game=root/'Game Package résumé'
for p in game.rglob('*'):
 if p.is_file():p.chmod(0o555 if p.name=='judas' else 0o444)
for p in sorted((p for p in game.rglob('*') if p.is_dir()),key=lambda p:len(p.parts),reverse=True):p.chmod(0o555)
game.chmod(0o555)
steps=root/'game.steps';steps.write_text('FRAMES 120\nFRAME_SECONDS 0.016666667\nLOG_EVERY 0\nEXPECT_SCENE Scenes/main.judas 100\nSCREENSHOT 110 '+str(logs/'first-project-export.png')+'\n')
# The game marker must select packaged resources even with a bad source hint.
run('first-project-standalone',[root/'Game Package résumé/judas'],extra={'JUDAS_ENGINE_ROOT':'/not-a-judas-source','JUDAS_TEST_SCRIPT':str(steps),'DISPLAY':':0','XDG_RUNTIME_DIR':'/run/user/1000','SDL_VIDEODRIVER':'x11'})
run('sdk-editor-new-project',[sdk/'judas_editor'],extra={'JUDAS_EDITOR_STABILIZATION':str(root/'Editor New Project'),'DISPLAY':':0','XDG_RUNTIME_DIR':'/run/user/1000','SDL_VIDEODRIVER':'x11'})
# Run visible starter code through ordinary editor Play/Stop without touching source.
run('sdk-editor-visible-project',[sdk/'judas_editor',project],extra={'JUDAS_EDITOR_AUTOTEST':str(root/'Visible Editor'),'DISPLAY':':0','XDG_RUNTIME_DIR':'/run/user/1000','SDL_VIDEODRIVER':'x11'})
assert hashes(sdk)==before,'SDK editor/runtime wrote into the installation'
(logs/'SDK_PAYLOAD_UNCHANGED.json').write_text(json.dumps({'files':len(before),'unchanged':True,'readOnlyInstall':True,'location':str(sdk),'unrelatedCwd':str(cwd),'sourceRootEnvironmentAbsentExceptInvalidStandaloneNegativeHint':True},indent=2)+'\n')
print('Linux SDK onboarding/export complete',flush=True)
