from pathlib import Path
import subprocess,json,time,importlib.util,shutil,hashlib,datetime
r=Path.cwd();o=r/'.cache/prealpha-final-qualification';source=o/'source';build=o/'build24';work=o/'sdk-work-final';sdk=o/'sdk-final';results=[]
while not (o/'CLEAN_BUILD_COMPLETE.json').exists():time.sleep(5)
assert json.loads((o/'REGRESSION_RESULT.json').read_text())['exit']==0,'Regression runner failed'
work.mkdir(exist_ok=True)
if not sdk.exists():shutil.copytree(build/'linux-sdk',sdk)
spec=importlib.util.spec_from_file_location('baseline',source/'tools/build_linux_baseline.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
rootfs=r/'.cache/prealpha-elastic-linux/ubuntu24'
prefix=m.environment_command(sdk,work,rootfs=rootfs)
def run(name,command,expected=0):
 start=time.monotonic()
 with (work/(name+'.log')).open('w') as f:
  try:code=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,timeout=180).returncode
  except subprocess.TimeoutExpired:code='timeout'
 results.append(dict(name=name,command=command,exit=code,expected=expected,seconds=time.monotonic()-start,utc=datetime.datetime.now(datetime.timezone.utc).isoformat()))
 (work/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(name,code,flush=True)
 assert code==expected,name
run('sdk-verify',prefix+['python3','/source/tools/package_sdk.py','verify','/source'])
run('sdk-inspect',prefix+['python3','/source/tools/inspect_sdk.py','/source','--host-check','--max-glibc','2.39','--require-os','ubuntu:24.04','--output','/build/SDK_QUALIFICATION.json'])
# Corrupt only the owned copy's manifest, then restore exact bytes.
manifest=sdk/'SDK_MANIFEST.json';before=manifest.read_bytes()
try:
 manifest.write_text('{}\n')
 run('corrupt-manifest-negative',prefix+['python3','/source/tools/package_sdk.py','verify','/source'],1)
finally:manifest.write_bytes(before)
run('sdk-verify-restored',prefix+['python3','/source/tools/package_sdk.py','verify','/source'])
script='''#!/bin/sh
set -eu
export SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 JUDAS_WORLD_STATE=none
export XDG_CONFIG_HOME=/build/editor-config JUDAS_EDITOR_AUTOTEST=/build/editor
export JUDAS_EDITOR_AUTOTEST_POLISH=1 JUDAS_EDITOR_AUTOTEST_NEW_PROJECT=/build/game
export JUDAS_EDITOR_AUTOTEST_EXPORT=/build/editor-export
cd /tmp
exec xvfb-run -a /source/judas_editor
'''
(work/'editor.sh').write_text(script);run('editor-create-export',prefix+['/bin/sh','/build/editor.sh'])
script='''#!/bin/sh
set -eu
export SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 JUDAS_WORLD_STATE=none
export XDG_CONFIG_HOME=/build/editor-play-config JUDAS_EDITOR_AUTOTEST=/build/editor-play
cd /tmp
exec xvfb-run -a /source/judas_editor /build/game/SDK_First_Game.judasproj
'''
(work/'play.sh').write_text(script);run('editor-play-stop',prefix+['/bin/sh','/build/play.sh'])
old=Path('/tmp/judas-sdk-final-tool-workflows.py').read_text()
old=old.replace("root=Path('/home/conner/Documents/GitHub/Project-Judas/.cache/prealpha-linux-readiness')","root=Path('/build')").replace("sdk=root/'sdk-final'; project=root/'sdk-final-game/SDK_First_Game.judasproj'","sdk=Path('/source'); project=root/'game/SDK_First_Game.judasproj'").replace("root/'sdk-final-cli-export'","root/'cli-export'")
(work/'tool-workflows.py').write_text(old);run('tool-workflows',prefix+['python3','/build/tool-workflows.py'])
run('sdk-verify-after-workflows',prefix+['python3','/source/tools/package_sdk.py','verify','/source'])
# Each standalone runs in a separate namespace with only its own package and
# tiny driver mounted. The source checkout, build tree and SDK are not mounted.
for name in ('editor-export','cli-export'):
 moved=o/('final-moved-'+name);shutil.move(str(work/name),moved)
 driver=o/('final-driver-'+name);driver.mkdir();(driver/'smoke.test').write_text('STEPS 90\nLOG_EVERY 30\nSCREENSHOT 89 /build/moved.png\n')
 (driver/'run.sh').write_text('''#!/bin/sh
set -eu
export SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 JUDAS_WORLD_STATE=none
export JUDAS_TEST_SCRIPT=/build/smoke.test
cd /tmp
exec xvfb-run -a /source/judas /source/game.judasproj
''')
 p=m.environment_command(moved,driver,rootfs=rootfs)
 run('moved-'+name,p+['/bin/sh','/build/run.sh'])
 assert (driver/'moved.png').is_file(),'No rendered moved screenshot'
 assert hashlib.sha256((moved/'judas').read_bytes()).digest()==hashlib.sha256((sdk/'judas').read_bytes()).digest(),'Export runtime differs from SDK'
(work/'COMPLETE.json').write_text(json.dumps({'passed':True,'sdk':str(sdk),'build_identity':json.loads((sdk/'BUILD_INFO.json').read_text()),'note':'Read-only SDK-only namespace for editor/tools; separate package-only namespace for relocated launches. Ubuntu24 llvmpipe, dummy audio. No original source/build/SDK mounts in moved-game runs.'},indent=2)+'\n')
