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


def qualify_alpha_v2(sdk, work, env, run):
    """Exercise the packaged, skinned groom project and a live API round trip."""
    project = work/'hair-project'
    shutil.copytree(sdk/'projects/hair_fur_lab', project)
    project_file = project/'hair_fur_lab.judasproj'
    for backend in ('opengl', 'vulkan'):
        editor_env = dict(env, JUDAS_GRAPHICS=backend, JUDAS_VULKAN_VALIDATION='1',
                         JUDAS_EDITOR_AUTOTEST=str(work/f'hair-editor-{backend}'),
                         JUDAS_EDITOR_AUTOTEST_SELECT='31',
                         XDG_CONFIG_HOME=str(work/f'hair-config-{backend}'))
        name = f'hair-editor-{backend}'
        run(name, [sdk/'judas_editor', project_file], editor_env, vulkan_graphics=backend=='vulkan')
        log = (work/(name+'.log')).read_text(errors='replace')
        if 'undo restores: IDENTICAL' not in log or 'after play/stop is IDENTICAL' not in log:
            raise ValueError(f'{backend} groom editor did not complete undo and Play/Stop')

    script = project/'Assets/scripts/showcase.js'
    source = script.read_text(encoding='utf-8')
    if source.count('export default class {') != 1:
        raise ValueError('Hair lab switch probe requires a single showcase class')
    script.write_text(source.replace('export default class {', 'class Showcase {') + r'''
import {settings, ui} from 'judas';
Showcase.prototype.update = function () {
    const state = settings.read();
    if (!state || state.backendLocked) return;
    if (!this.switchStage && this.state.elapsed >= 0.15) {
        if (state.activeBackend !== 'opengl' || !state.liveBackendSwitch)
            throw new Error('Packaged runtime did not offer a live OpenGL to Vulkan switch');
        this.marker = {elapsed: this.state.elapsed};
        this.switchStage = 'to-vulkan';
        settings.switchBackend('vulkan');
    } else if (this.switchStage === 'to-vulkan' && state.backendTransition === 'completed') {
        if (state.activeBackend !== 'vulkan' || !this.marker)
            throw new Error('Vulkan did not retain the showcase instance');
        this.switchStage = 'to-opengl';
        settings.switchBackend('opengl');
    } else if (this.switchStage === 'to-opengl' && state.backendTransition === 'completed') {
        if (state.activeBackend !== 'opengl' || !world.entity('31')?.valid ||
            this.state.elapsed < this.marker.elapsed)
            throw new Error('OpenGL did not restore the same groom scene and script');
        console.log('ALPHA_V2_ROUNDTRIP_OK');
        ui.quit();
    } else if (this.switchStage && state.backendError) {
        throw new Error(state.backendError);
    }
};
export default Showcase;
''', encoding='utf-8')
    package = work/'hair-export'
    run('hair-export', [sdk/'judas_export', project_file, package, sdk/'judas'])
    moved = work/'hair-relocated'
    package.rename(moved)
    exe = moved/'judas'
    with exe.open('rb') as exported, (sdk/'judas').open('rb') as packaged:
        if hashlib.file_digest(exported, 'sha256').digest() != hashlib.file_digest(packaged, 'sha256').digest():
            raise ValueError('Hair export runtime differs from the candidate SDK')
    # Explicit backend launches exercise the same standalone archive that is
    # then run without a launch override for the in-process switch.
    for backend in ('opengl', 'vulkan'):
        image = work/f'hair-{backend}.png'
        harness = work/f'hair-{backend}.test'
        harness.write_text(f'FRAMES 95\nSCREENSHOT 85 {image}\n', encoding='utf-8')
        run(f'hair-runtime-{backend}', [exe, moved/'game.judasproj'],
            dict(env, JUDAS_GRAPHICS=backend, JUDAS_VULKAN_VALIDATION='1', JUDAS_TEST_SCRIPT=str(harness)),
            vulkan_graphics=backend=='vulkan')
        if not image.is_file() or 'SCRIPT STOPPED' in (work/f'hair-runtime-{backend}.log').read_text(errors='replace'):
            raise ValueError(f'{backend} relocated groom scene did not render')
    run('hair-live-backend-switch', [exe, moved/'game.judasproj'],
        dict(env, JUDAS_VULKAN_VALIDATION='1', XDG_DATA_HOME=str(work/'switch-user')), vulkan_graphics=True)
    log = (work/'hair-live-backend-switch.log').read_text(errors='replace')
    if 'ALPHA_V2_ROUNDTRIP_OK' not in log or 'SCRIPT STOPPED' in log:
        raise ValueError('Packaged runtime did not complete OpenGL → Vulkan → OpenGL')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk',type=Path,required=True);p.add_argument('--work',type=Path,required=True)
    p.add_argument('--rootfs',type=Path);p.add_argument('--mesa',type=Path)
    p.add_argument('--vulkan', action='store_true', help='Exercise the native Linux Vulkan SDK with validation enabled')
    p.add_argument('--alpha-v2', action='store_true', help='Exercise the packaged V2 groom editor, export and live backend switch')
    a=p.parse_args();sdk=a.sdk.resolve();work=a.work.resolve();work.mkdir(exist_ok=False,parents=True)
    windows=os.name=='nt';ext='.exe' if windows else '';rows=[]
    identity=json.loads((sdk/'BUILD_INFO.json').read_text())
    if a.vulkan and (windows or a.rootfs or not identity.get('vulkan')):
        p.error('--vulkan requires a Vulkan-enabled SDK on a native Linux host')
    if a.alpha_v2 and not a.vulkan:
        p.error('--alpha-v2 requires both compiled backends and --vulkan')
    spec=importlib.util.spec_from_file_location('baseline',ROOT/'tools/build_linux_baseline.py');baseline=importlib.util.module_from_spec(spec);spec.loader.exec_module(baseline)
    shutil.copytree(ROOT/'tests/fixtures/rendering',work/'fixtures')
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
    base_env.update(SDL_AUDIODRIVER='dummy',PYTHONUTF8='1')
    graphics_env=dict(base_env)
    if a.vulkan:
        graphics_env.update(JUDAS_GRAPHICS='vulkan', JUDAS_VULKAN_VALIDATION='1')
    def run(name,command,env=None,expected=0,vulkan_graphics=False):
        start=time.monotonic()
        with (work/(name+'.log')).open('w',encoding='utf-8') as f:
            result=subprocess.run(list(map(str,command)),cwd=work,env=env or base_env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
        rows.append(dict(name=name,command=list(map(str,command)),exit=result.returncode,expected=expected,seconds=time.monotonic()-start))
        (work/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n')
        if result.returncode!=expected:raise RuntimeError(name+' failed; inspect '+str(work/(name+'.log')))
        if vulkan_graphics:
            log=(work/(name+'.log')).read_text(errors='replace')
            if not any(line.startswith('[Graphics] Vulkan backend:') and 'validation on' in line
                       for line in log.splitlines()) or '[Vulkan validation]' in log:
                raise RuntimeError(name+' did not complete Vulkan validation cleanly')
    py='python3' if a.rootfs else sys.executable
    run('sdk-verify',prefix()+[py,inner_sdk/'tools/package_sdk.py','verify',inner_sdk])
    inspect=[py,inner_sdk/'tools/inspect_sdk.py',inner_sdk,'--host-check','--output',inner_work/'SDK_QUALIFICATION.json']
    if a.rootfs:inspect+=['--max-glibc','2.39','--require-os','ubuntu:24.04']
    if windows:inspect+=['--system-directory',os.environ.get('SystemRoot','C:/Windows')+'/System32']
    run('sdk-inspect',prefix()+inspect)
    for mode in ['create','play']:
        env=dict(graphics_env,XDG_CONFIG_HOME=str(work/('editor-config-'+mode)),JUDAS_EDITOR_AUTOTEST=str(work/('editor-'+mode)))
        command=[active/('judas_editor'+ext)]
        if mode=='create':env.update(JUDAS_EDITOR_AUTOTEST_POLISH='1',JUDAS_EDITOR_AUTOTEST_NEW_PROJECT=str(work/'game'),JUDAS_EDITOR_AUTOTEST_EXPORT=str(work/'editor-export'))
        else:command+=[work/'game/SDK_First_Game.judasproj']
        if a.rootfs:
            script='set -eu\nexport SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1\n'
            script+=f'export XDG_CONFIG_HOME=/build/editor-config-{mode} JUDAS_EDITOR_AUTOTEST=/build/editor-{mode}\n'
            if mode=='create':script+='export JUDAS_EDITOR_AUTOTEST_POLISH=1 JUDAS_EDITOR_AUTOTEST_NEW_PROJECT=/build/game JUDAS_EDITOR_AUTOTEST_EXPORT=/build/editor-export\n'
            script+='cd /tmp\nexec xvfb-run -a /source/judas_editor'+(' /build/game/SDK_First_Game.judasproj' if mode=='play' else '')+'\n'
            (work/(mode+'.sh')).write_text(script)
            run('editor-'+mode,prefix()+['/bin/sh',inner_work/(mode+'.sh')])
        else:run('editor-'+mode,command,env,vulkan_graphics=a.vulkan)
    tool_workflows=prefix()+[py,inner_work/'tool-workflows.py','--sdk',inner_sdk,'--work',inner_work]
    if a.vulkan:
        tool_workflows.append('--vulkan')
    run('tool-operations',tool_workflows)
    # SDK-only operations must not create caches, alter binaries or add content.
    run('sdk-verify-after',prefix()+[py,inner_sdk/'tools/package_sdk.py','verify',inner_sdk])
    for name in ['editor-export','cli-export']:
        source=work/name;moved=work/('moved-'+name);shutil.move(str(source),moved)
        expected=hashlib.sha256((sdk/('judas'+ext)).read_bytes()).hexdigest()
        if hashlib.sha256((moved/('judas'+ext)).read_bytes()).hexdigest()!=expected:raise ValueError('Export runtime differs from SDK')
        if identity.get('vulkan'):
            compiler_notices='third_party/vulkan'
            for notice in (sdk/compiler_notices).rglob('*'):
                if notice.is_file():
                    relative=notice.relative_to(sdk)
                    if (moved/'engine'/relative).read_bytes()!=notice.read_bytes():
                        raise ValueError('Export compiler notice differs from SDK: '+str(relative))
        driver=work/('driver-'+name);driver.mkdir()
        (driver/'smoke.test').write_text('STEPS 90\nLOG_EVERY 30\nSCREENSHOT 89 '+('/build/moved.png' if a.rootfs else str(driver/'moved.png'))+'\n')
        if a.rootfs:
            (driver/'run.sh').write_text('set -eu\nexport SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 JUDAS_TEST_SCRIPT=/build/smoke.test\ncd /tmp\nexec xvfb-run -a /source/judas /source/game.judasproj\n')
            run('relocated-'+name,prefix(moved,driver)+['/bin/sh','/build/run.sh'])
        else:
            launch=moved
            if a.mesa:
                launch=work/('software-'+name);shutil.copytree(moved,launch)
                for dll in a.mesa.glob('*.dll'):shutil.copyfile(dll,launch/dll.name)
            run('relocated-'+name,[launch/('judas'+ext),launch/'game.judasproj'],
                dict(graphics_env,JUDAS_TEST_SCRIPT=str(driver/'smoke.test')),vulkan_graphics=a.vulkan)
        if not (driver/'moved.png').is_file():raise ValueError('Relocated game did not render a frame')
    if a.alpha_v2:
        qualify_alpha_v2(sdk, work, base_env, run)
        run('sdk-verify-after-alpha-v2',[py, sdk/'tools/package_sdk.py','verify',sdk])
    (work/'COMPLETE.json').write_text(json.dumps({'passed':True,'build':identity,'sourceOnlyMounted':False,'packageOnlyNamespaces':bool(a.rootfs),'softwareGraphics':bool(a.rootfs or a.mesa),'hardwareAcceptance':False,'operations':len(rows),'alphaV2':a.alpha_v2},indent=2)+'\n')
    print('SDK workflows passed:',work)

if __name__=='__main__':main()
