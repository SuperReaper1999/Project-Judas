#!/usr/bin/env python3
"""Experimental native-WASM game export. Explicit per-invocation opt-in required."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[1]

def run(command,**kwargs):
    print('+',subprocess.list2cmdline(list(map(str,command))),flush=True)
    subprocess.run(list(map(str,command)),check=True,**kwargs)

def probe(path):
    return json.loads(subprocess.check_output([str(path),'--build-info-json'],text=True))

def parse_args(argv=None):
    p=argparse.ArgumentParser(description=__doc__,allow_abbrev=False)
    p.add_argument('-ExpBrowserSupport',choices=['0','1'],default='0')
    p.add_argument('--project',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--emsdk',type=Path,default=Path(os.environ.get('EMSDK',ROOT/'.cache/browser-sdk/emsdk')))
    p.add_argument('--build',type=Path,default=ROOT/'.cache/browser-build')
    p.add_argument('--exporter',type=Path,default=ROOT/'build/judas_export')
    p.add_argument('--desktop-runtime',type=Path,default=ROOT/'build/judas')
    p.add_argument('--icu-host',type=Path)
    p.add_argument('--jobs',type=int,choices=range(1,9),default=2)
    words=list(sys.argv[1:] if argv is None else argv)
    # Match the editor's documented separated spelling, while refusing duplicate
    # switches rather than letting argparse's last value silently win.
    if sum(word=='-ExpBrowserSupport' or word.startswith('-ExpBrowserSupport=') for word in words)>1:
        p.error('Specify -ExpBrowserSupport once')
    for i in range(len(words)-2):
        if words[i:i+2]==['-ExpBrowserSupport','=']:
            words[i:i+3]=['-ExpBrowserSupport='+words[i+2]]
            break
    a=p.parse_args(words)
    exact=any(word=='-ExpBrowserSupport=1' or (word=='-ExpBrowserSupport' and i+1<len(words) and words[i+1]=='1') for i,word in enumerate(words))
    if a.ExpBrowserSupport=='1' and not exact:p.error('Spell the explicit opt-in -ExpBrowserSupport=1 in full')
    if a.ExpBrowserSupport!='1':p.error('Browser export is hidden/disabled; explicitly pass -ExpBrowserSupport=1 to opt in')
    if sys.platform!='linux':p.error('The initial experimental browser compiler workflow requires a Linux host')
    return a

def require_supported_project(project_text):
    if project_text.lstrip().startswith('{'):
        document=json.loads(project_text)
        if not isinstance(document,dict) or not isinstance(document.get('data'),dict):
            raise ValueError('Expected a named project data object')
        policy=document['data']
        unsupported=policy.get('advancedFluidDynamics',False) or policy.get('legacyGameplay',True)
    else:
        import shlex
        fields={line[0]:line[1:] for raw in project_text.splitlines() if (line:=shlex.split(raw))}
        unsupported=fields.get('advanced-fluid-dynamics',['false'])!=['false'] or fields.get('legacy-gameplay',['true'])!=['false']
    if unsupported:
        raise ValueError('Browser projects require script-owned gameplay and Advanced Fluid Dynamics disabled')

def copy_browser_licenses(sdk,output):
    destination=output/'licenses'
    destination.mkdir()
    shutil.copy2(ROOT/'LICENSE',destination/'Judas-LICENSE.txt')
    shutil.copy2(ROOT/'third_party/RUNTIME_NOTICES.txt',destination/'ENGINE_NOTICES.txt')
    for name in ['SDL-LICENSE.txt','GLM-COPYING.txt']:
        shutil.copy2(ROOT/'third_party/android'/name,destination/name)
    runtime={
        'emscripten-LICENSE.txt':'LICENSE',
        'musl-COPYRIGHT.txt':'system/lib/libc/musl/COPYRIGHT',
        'compiler-rt-LICENSE.txt':'system/lib/compiler-rt/LICENSE.TXT',
        'libcxx-LICENSE.txt':'system/lib/libcxx/LICENSE.TXT',
        'libcxxabi-LICENSE.txt':'system/lib/libcxxabi/LICENSE.TXT',
        'libunwind-LICENSE.txt':'system/lib/libunwind/LICENSE.TXT',
    }
    for name,relative in runtime.items():
        if not (sdk/relative).is_file():
            raise ValueError('Emscripten runtime redistribution notice missing: '+relative)
        shutil.copy2(sdk/relative,destination/name)

def main(argv=None):
    a=parse_args(argv)
    if a.output.is_symlink():raise ValueError('Browser output must not be a symlink')
    for name in ['project','output','emsdk','build','exporter','desktop_runtime']:
        setattr(a,name,getattr(a,name).resolve())
    if not a.project.is_file():raise ValueError('Project file does not exist')
    if a.output.exists():raise ValueError('Browser output must be a fresh directory; existing projects/packages are never replaced')
    if a.output==a.project.parent or a.project.parent in a.output.parents:
        raise ValueError('Browser output must be outside the project')
    sdk=a.emsdk/'upstream/emscripten'
    emcmake=sdk/'emcmake'
    if not emcmake.is_file():raise ValueError('Install/activate the documented Emscripten SDK; --emsdk must name its root')
    environment=dict(os.environ,EM_CONFIG=str(a.emsdk/'.emscripten'))
    environment['PATH']=str(sdk)+os.pathsep+environment.get('PATH','')
    native={name:probe(path) for name,path in [('exporter',a.exporter),('runtime',a.desktop_runtime)]}
    identity_fields=['version','source_revision','source_fingerprint','source_untracked_fingerprint','channel']
    if any(native['exporter'].get(f)!=native['runtime'].get(f) for f in identity_fields):
        raise ValueError('Native exporter and runtime identities differ; rebuild both from this source')
    a.build.mkdir(parents=True,exist_ok=True)
    lock=a.build/'.browser-export.lock'
    try:fd=os.open(lock,os.O_CREAT|os.O_EXCL|os.O_WRONLY,0o600)
    except FileExistsError:raise ValueError('Browser build is in use; inspect the recorded PID before removing a stale lock')
    os.write(fd,str(os.getpid()).encode());os.close(fd)
    stage=a.build/('export-'+uuid.uuid4().hex)
    stage.mkdir()
    try:
        project_text=a.project.read_text(encoding='utf-8')
        require_supported_project(project_text)
        command=[emcmake,'cmake','-S',ROOT,'-B',a.build,'-DCMAKE_BUILD_TYPE=Release','-DJUDAS_BUILD_CHANNEL='+native['runtime']['channel']]
        if a.icu_host:command+=['-DJUDAS_BROWSER_ICU_HOST_TOOLS='+str(a.icu_host.resolve())]
        run(command,env=environment)
        planned=json.loads((a.build/'judas-build-info-Release.json').read_text(encoding='utf-8'))
        if any(planned.get(f)!=native['runtime'].get(f) for f in identity_fields):
            raise ValueError('Current source and native tools differ; rebuild matching native tools before browser compilation')
        run(['cmake','--build',a.build,'--target','judas_browser','--parallel',a.jobs],env=environment)
        # Query the actual compiled WASM, not just its CMake sidecar. This main
        # path exits before any canvas or game initialization, and runs in Node.
        wasm=json.loads(subprocess.check_output(['node',str(a.build/'judas_browser.js'),'--build-info-json'],text=True,env=environment))
        run(command,env=environment)
        current=json.loads((a.build/'judas-build-info-Release.json').read_text(encoding='utf-8'))
        if any(wasm.get(f)!=current.get(f) for f in identity_fields):
            raise ValueError('Source changed during browser compilation; refusing a mixed-source package')
        if any(wasm.get(f)!=native['runtime'].get(f) for f in identity_fields):
            raise ValueError('Browser engine and native tools differ in source/version/channel; rebuild matching native tools before export')
        for name,path in [('exporter',a.exporter),('runtime',a.desktop_runtime)]:
            if probe(path)!=native[name]:raise ValueError('Native tools changed during browser compilation')
        package=stage/'native-content'
        run([a.exporter,a.project,package,a.desktop_runtime])
        content=stage/'content';content.mkdir()
        ignored={'judas','judas.exe','judas-runtime.txt','BUILD_PROVENANCE.json','BUILD_INFO.json'}
        for path in sorted(package.rglob('*')):
            if not path.is_file():continue
            rel=path.relative_to(package)
            if rel.parts[0] in ignored:continue
            if len(rel.parts)==1 and (path.suffix in ('.so','.dll','.exe') or '.so.' in path.name):continue
            target=content/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
        output=stage/'web';output.mkdir()
        for path in a.build.glob('judas_browser.*'):
            if path.suffix in ('.js','.wasm'):shutil.copy2(path,output/path.name)
        shutil.copy2(a.build/'judas_browser.html',output/'index.html')
        run([sys.executable,sdk/'tools/file_packager.py',output/'game.data','--preload',str(content)+'@/game','--js-output='+str(output/'game.data.js')],env=environment)
        marker=(content/'judas-package.txt')
        if not marker.exists():
            # Use the public launch marker name declared by the engine, not a
            # guessed save identity from project-directory paths.
            markers=[p for p in content.iterdir() if p.is_file() and p.read_bytes().startswith(b'JudasPackage 1\n')]
            if len(markers)!=1:raise ValueError('Exported content has no unique public game launch marker')
            marker=markers[0]
        import shlex
        launch=shlex.split(marker.read_text(encoding='utf-8'))
        save_id=launch[launch.index('save-id')+1]
        (output/'game-config.js').write_text('const JUDAS_GAME='+json.dumps({'saveIdentity':save_id,'controls':'Click the canvas to focus. Use your game controls; reload this page to restart.'},ensure_ascii=True)+';\n',encoding='utf-8')
        copy_browser_licenses(sdk,output)
        # Preserve the native export's exact redistribution inventory in a
        # readable web directory as well as the virtual game filesystem.
        if (content/'engine/licenses').is_dir():
            shutil.copytree(content/'engine/licenses',output/'engine/licenses')
        (output/'README.txt').write_text(
            'Judas experimental browser game. Requires WebGL2, SharedArrayBuffer, IndexedDB and Web Locks.\n'
            'Serve on HTTP localhost or HTTPS with COOP same-origin and COEP require-corp.\n'
            'Do not open index.html via file://. Keep every file/directory together.\n'
            'This is not universal browser/device qualification. Saves belong to this origin/profile.\n'
            'Engine and web runtime redistribution notices: licenses/; exported game/font notices: engine/licenses/.\n'
            'This WASM build statically links pinned SDL2/GLM. Native-platform remarks in ENGINE_NOTICES describe native packages.\n',encoding='utf-8')
        receipt={'schema':1,'kind':'judas-browser-experimental','engine':wasm,'nativeTools':native,'experimentalOptIn':True,'requires':['WebAssembly','WebGL2','cross-origin isolation','SharedArrayBuffer','IndexedDB','Web Locks'],'hosting':'HTTP localhost or HTTPS with COOP same-origin and COEP require-corp','files':{p.relative_to(output).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(output.rglob('*')) if p.is_file()}}
        (output/'BROWSER_BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
        a.output.parent.mkdir(parents=True,exist_ok=True)
        if a.output.exists():raise ValueError('Output appeared during build; refusing replacement')
        shutil.copytree(output,a.output)
        print('Experimental browser package:',a.output)
        print('Serve with: python3 tools/serve_browser.py',a.output)
    finally:
        lock.unlink(missing_ok=True)

if __name__=='__main__':
    try:main()
    except (OSError,ValueError,subprocess.CalledProcessError) as error:
        print('Browser export failed:',error,file=sys.stderr);sys.exit(1)
