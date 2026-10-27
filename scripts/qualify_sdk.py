#!/usr/bin/env python3
"""Qualify a native SDK through actual authoring, Play/Stop and moved games.

Linux --rootfs isolates the SDK and each export in separate read-only namespaces.
Windows can use an explicitly supplied test-only Mesa directory in disposable
copies. No driver DLL is added to the stock SDK or pristine exports.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
sys.dont_write_bytecode=True


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk',type=Path,required=True);p.add_argument('--work',type=Path,required=True)
    p.add_argument('--rootfs',type=Path);p.add_argument('--mesa',type=Path)
    a=p.parse_args();sdk=a.sdk.resolve();work=a.work.resolve();work.mkdir(exist_ok=False,parents=True)
    windows=os.name=='nt';ext='.exe' if windows else '';rows=[]
    identity=json.loads((sdk/'BUILD_INFO.json').read_text())
    spec=importlib.util.spec_from_file_location('baseline',ROOT/'tools/build_linux_baseline.py');baseline=importlib.util.module_from_spec(spec);spec.loader.exec_module(baseline)
    shutil.copytree(ROOT/'tests/fixtures/releases/sdk',work/'fixtures')
    shutil.copyfile(ROOT/'scripts/qualify_sdk_workflows.py',work/'tool-workflows.py')
    active=sdk
    if a.mesa:
        if not windows:raise ValueError('--mesa is only for native Windows test copies')
        active=work/'software-graphics-sdk';shutil.copytree(sdk,active)
        for dll in a.mesa.glob('*.dll'):shutil.copyfile(dll,active/dll.name)
    def prefix(source=sdk,destination=work):
        return baseline.environment_command(source,destination,rootfs=a.rootfs) if a.rootfs else []
    inner_sdk=Path('/source') if a.rootfs else sdk
    inner_active=Path('/source') if a.rootfs else active
    inner_work=Path('/build') if a.rootfs else work
    base_env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')}
    base_env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',PYTHONUTF8='1')
    def run(name,command,env=None,expected=0):
        start=time.monotonic()
        with (work/(name+'.log')).open('w',encoding='utf-8') as f:
            result=subprocess.run(list(map(str,command)),cwd=work,env=env or base_env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
        rows.append(dict(name=name,command=list(map(str,command)),exit=result.returncode,expected=expected,seconds=time.monotonic()-start))
        (work/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n')
        if result.returncode!=expected:raise RuntimeError(name+' failed; inspect '+str(work/(name+'.log')))
    py='python3' if a.rootfs else sys.executable
    run('sdk-verify',prefix()+[py,inner_sdk/'tools/package_sdk.py','verify',inner_sdk])
    inspect=[py,inner_sdk/'tools/inspect_sdk.py',inner_sdk,'--host-check','--output',inner_work/'SDK_QUALIFICATION.json']
    if a.rootfs:inspect+=['--max-glibc','2.39','--require-os','ubuntu:24.04']
    if windows:inspect+=['--system-directory',os.environ.get('SystemRoot','C:/Windows')+'/System32']
    run('sdk-inspect',prefix()+inspect)
    for mode in ['create','play']:
        env=dict(base_env,XDG_CONFIG_HOME=str(work/('editor-config-'+mode)),JUDAS_EDITOR_AUTOTEST=str(work/('editor-'+mode)))
        command=[active/('judas_editor'+ext)]
        if mode=='create':env.update(JUDAS_EDITOR_AUTOTEST_POLISH='1',JUDAS_EDITOR_AUTOTEST_NEW_PROJECT=str(work/'game'),JUDAS_EDITOR_AUTOTEST_EXPORT=str(work/'editor-export'))
        else:command+=[work/'game/SDK_First_Game.judasproj']
        if a.rootfs:
            script='set -eu\nexport SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 JUDAS_WORLD_STATE=none\n'
            script+=f'export XDG_CONFIG_HOME=/build/editor-config-{mode} JUDAS_EDITOR_AUTOTEST=/build/editor-{mode}\n'
            if mode=='create':script+='export JUDAS_EDITOR_AUTOTEST_POLISH=1 JUDAS_EDITOR_AUTOTEST_NEW_PROJECT=/build/game JUDAS_EDITOR_AUTOTEST_EXPORT=/build/editor-export\n'
            script+='cd /tmp\nexec xvfb-run -a /source/judas_editor'+(' /build/game/SDK_First_Game.judasproj' if mode=='play' else '')+'\n'
            (work/(mode+'.sh')).write_text(script)
            run('editor-'+mode,prefix()+['/bin/sh',inner_work/(mode+'.sh')])
        else:run('editor-'+mode,command,env)
    run('tool-operations',prefix()+[py,inner_work/'tool-workflows.py','--sdk',inner_sdk,'--work',inner_work])
    # SDK-only operations must not create caches, alter binaries or add content.
    run('sdk-verify-after',prefix()+[py,inner_sdk/'tools/package_sdk.py','verify',inner_sdk])
    for name in ['editor-export','cli-export']:
        source=work/name;moved=work/('moved-'+name);shutil.move(str(source),moved)
        expected=hashlib.sha256((sdk/('judas'+ext)).read_bytes()).hexdigest()
        if hashlib.sha256((moved/('judas'+ext)).read_bytes()).hexdigest()!=expected:raise ValueError('Export runtime differs from SDK')
        driver=work/('driver-'+name);driver.mkdir()
        (driver/'smoke.test').write_text('STEPS 90\nLOG_EVERY 30\nSCREENSHOT 89 '+('/build/moved.png' if a.rootfs else str(driver/'moved.png'))+'\n')
        if a.rootfs:
            (driver/'run.sh').write_text('set -eu\nexport SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 JUDAS_WORLD_STATE=none JUDAS_TEST_SCRIPT=/build/smoke.test\ncd /tmp\nexec xvfb-run -a /source/judas /source/game.judasproj\n')
            run('relocated-'+name,prefix(moved,driver)+['/bin/sh','/build/run.sh'])
        else:
            launch=moved
            if a.mesa:
                launch=work/('software-'+name);shutil.copytree(moved,launch)
                for dll in a.mesa.glob('*.dll'):shutil.copyfile(dll,launch/dll.name)
            run('relocated-'+name,[launch/('judas'+ext),launch/'game.judasproj'],dict(base_env,JUDAS_TEST_SCRIPT=str(driver/'smoke.test')))
        if not (driver/'moved.png').is_file():raise ValueError('Relocated game did not render a frame')
    (work/'COMPLETE.json').write_text(json.dumps({'passed':True,'build':identity,'sourceOnlyMounted':False,'packageOnlyNamespaces':bool(a.rootfs),'softwareGraphics':bool(a.rootfs or a.mesa),'hardwareAcceptance':False,'operations':len(rows)},indent=2)+'\n')
    print('SDK workflows passed:',work)

if __name__=='__main__':main()
