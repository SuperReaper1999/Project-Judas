#!/usr/bin/env python3
"""Bounded native Pass4 qualification; original fixtures are not rewritten."""
from pathlib import Path
import argparse, datetime, hashlib, json, os, platform, re, shlex, shutil, subprocess, time

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / '.cache/prealpha-pass4/linux'
EV = ROOT / 'docs/evidence/prealpha-pass4/linux'
ENV = os.environ.copy()
ENV.update(SDL_AUDIODRIVER='dummy', JUDAS_WORLD_STATE='none',
           XDG_DATA_HOME=str(OUT/'user-data'), XDG_CONFIG_HOME=str(OUT/'config'))
TARGETS = ['judas_linux_sdk', 'judas_character_lab_tests', 'judas_balance_lab_tests',
           'judas_origin_physics_tests', 'judas_origin_assembly_tests', 'judas_origin_stream_tests',
           'judas_prealpha_lifecycle_tests', 'judas_world_persistence_tests', 'judas_particle_application_tests']

def digest(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n')

def run(name, argv, *, expected=0, extra=None, cwd=ROOT, timeout=120):
    argv = [str(x) for x in argv]
    env = ENV.copy(); env.update(extra or {})
    row = dict(name=name, argv=argv, cwd=str(cwd), startedUtc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
               expectedExit=expected, environmentOverrides={k:env[k] for k in ['SDL_AUDIODRIVER','JUDAS_WORLD_STATE','XDG_DATA_HOME','XDG_CONFIG_HOME',*(extra or {})]})
    exe = Path(argv[0]); row['executableSha256'] = digest(exe) if exe.is_file() else None
    start=time.monotonic(); path=EV/'logs'/(name+'.log'); path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open('w') as stream:
            result = subprocess.run(argv, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=timeout)
        row.update(exit=result.returncode, exitMatchesExpectation=result.returncode==expected)
    except subprocess.TimeoutExpired:
        row.update(exit=None, exitMatchesExpectation=False, timedOut=True)
    row.update(seconds=time.monotonic()-start, log=str(path.relative_to(ROOT)), logSha256=digest(path))
    previous=json.loads((EV/'RESULTS.json').read_text()) if (EV/'RESULTS.json').exists() else []
    previous.append(row); save(EV/'RESULTS.json', previous)
    print(name, row['exit'], round(row['seconds'],3), flush=True)
    return row['exitMatchesExpectation']

def build():
    files=['CMakeLists.txt', 'src/PhysicsWorld.cpp', 'src/PhysicsWorld.h',
           *[str(p.relative_to(ROOT)) for p in sorted((ROOT/'src/physics').glob('*')) if p.is_file()],
           'tests/BalanceLabTests.cpp','tests/CharacterLabTests.cpp','tests/OriginPhysicsTests.cpp',
           'tests/OriginAssemblyTests.cpp','tests/OriginStreamTests.cpp','tests/PreAlphaLifecycleTests.cpp',
           'tests/WorldPersistenceTests.cpp','tests/ParticleApplicationTests.cpp',
           'docs/evidence/m78-followup/probes/concave_final.cpp', 'packaging/sdk-inventory.json']
    head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    assert head=='60a92176adbdfb8ccf90cc1fe3a40505416b1559', head
    start={p:digest(ROOT/p) for p in files}
    save(EV/'SOURCE_FINGERPRINTS.json',dict(head=head, files=start, hardware=platform.uname()._asdict(),
         cpuInfo=(Path('/proc/cpuinfo').read_text().split('\n\n')[0]),
         cmakeCacheSha256=digest(ROOT/'build/CMakeCache.txt')))
    assert run('build_sdk_and_focused', ['cmake','--build',ROOT/'build','--target',*TARGETS,'-j2'],timeout=600)
    assert start=={p:digest(ROOT/p) for p in files}, 'Source changed during build'
    shutil.copyfile(ROOT/'build/judas-build-info-Release.json', EV/'BUILD_INFO.json')
    binaries=TARGETS[1:]+json.loads((ROOT/'packaging/sdk-inventory.json').read_text())['tools']
    save(EV/'BINARY_FINGERPRINTS.json',dict(archiveSha256=digest(ROOT/'build/libjudas_engine.a'),
         binaries={p:digest(ROOT/'build'/p) for p in binaries}))

def functional():
    cases=[('origin_physics','judas_origin_physics_tests'),('origin_assembly','judas_origin_assembly_tests'),
           ('origin_stream','judas_origin_stream_tests'),('composed_lifecycle','judas_prealpha_lifecycle_tests'),
           ('world_persistence','judas_world_persistence_tests')]
    for name, target in cases: run(name,[ROOT/'build'/target])
    run('particle_application',[ROOT/'build/judas_particle_application_tests','--output',OUT/'particle-app'])
    run('m70_actual_physics',[ROOT/'build/judas_character_lab_tests',OUT/'m70-app'])
    run('m70_cold_write',[ROOT/'build/judas_character_lab_tests',OUT/'m70-cold','cold-write'])
    run('m70_cold_read',[ROOT/'build/judas_character_lab_tests',OUT/'m70-cold','cold-read'])
    for label, stem in [('step','cold'),('stand','cold-stand'),('recovery','cold-recovery')]:
        run('m77_'+label+'_write',[ROOT/'build/judas_balance_lab_tests',OUT/('m77-'+label),stem+'-write'],timeout=180)
        run('m77_'+label+'_read',[ROOT/'build/judas_balance_lab_tests',OUT/('m77-'+label),stem+'-read'],timeout=180)
    # Keep the durable state/real endpoint receipts; no projects or save binaries are staged.
    for directory in ['m70-app','m70-cold','m77-step','m77-stand','m77-recovery']:
        for p in (OUT/directory).glob('*.json'):
            destination=EV/'physical-endpoints'/directory/p.name
            destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,destination)

def performance():
    # Compile the byte-identical M78 frozen observer using current includes and final libraries.
    flags=(ROOT/'build/CMakeFiles/judas_character_lab_tests.dir/flags.make').read_text()
    options=[]
    for label in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:
        options+=shlex.split(re.search(r'^'+label+r' = (.*)$',flags,re.M).group(1))
    source=ROOT/'docs/evidence/m78-followup/probes/concave_final.cpp'
    assert digest(source)=='f0ce020eb47613ae82bb2619cdc03e6b477782b744576be2b9a36a52f5079136'
    obj=OUT/'four-ragdoll.o'; exe=OUT/'four-ragdoll'
    assert run('four_ragdoll_compile',['/usr/bin/c++',*options,'-c',source,'-o',obj],timeout=120)
    command=shlex.split((ROOT/'build/CMakeFiles/judas_character_lab_tests.dir/link.txt').read_text())
    command=[str(obj) if x=='CMakeFiles/judas_character_lab_tests.dir/tests/CharacterLabTests.cpp.o' else x for x in command]
    command=[x for x in command if not x.startswith('-Wl,--dependency-file=')]
    command[command.index('-o')+1]=str(exe)
    assert run('four_ragdoll_link',command,cwd=ROOT/'build',timeout=120)
    before=Path('/proc/loadavg').read_text().strip()
    run('four_ragdoll_run',[exe,OUT/'four-ragdoll-raw'],timeout=180,
        extra={'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'})
    raw=OUT/'four-ragdoll-raw'
    for p in raw.iterdir():
        destination=EV/'four-ragdoll'/p.name;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,destination)
    log=(EV/'logs/four_ragdoll_run.log').read_text()
    rows=[json.loads(x) for x in log.splitlines() if x.startswith('{"case"')]
    save(EV/'FOUR_RAGDOLL_RESULTS.json',dict(loadAverageBefore=before, rows=rows,
        sourceSha256=digest(source), executableSha256=digest(exe), archiveSha256=digest(ROOT/'build/libjudas_engine.a'),
        physicalObserverPassed='observer_sanity=1' in log,
        meshFourP95GateMs=16.67, meshFourP95Pass=next((r['p95Ms']<=16.67 for r in rows if r['case']=='mesh-4'),False),
        scope='Original unchanged four-case900-step native Linux physical workload, not GPU performance. Original p95 gate is reported separately from observer sanity.'))

def packaging():
    sdk=ROOT/'build/linux-sdk'
    run('sdk_verify',['python3',ROOT/'tools/package_sdk.py','verify',sdk])
    run('sdk_native_loader',['python3',ROOT/'tools/inspect_sdk.py',sdk,'--host-check','--output',EV/'SDK_QUALIFICATION.json'])
    # Positive and actual syntax-failure controls use private ordinary project copies.
    pristine=OUT/'stress-project';shutil.copytree(ROOT/'projects/physics_stress_lab',pristine)
    project=pristine/'physics_stress_lab.judasproj'
    run('sdk_script_check',[sdk/'judas_script_check',project])
    bad=OUT/'bad-script-project';shutil.copytree(pristine,bad)
    with (bad/'Assets/scripts/lab.js').open('a') as stream: stream.write('\nexport const brokenSyntax = ;\n')
    run('sdk_script_negative',[sdk/'judas_script_check',bad/project.name],expected=1)
    run('sdk_model_inspect',[sdk/'judas_model_import_cli','--inspect',ROOT/'projects/character_lab/Sources/figure.gltf'])
    run('sdk_collision_cook',[sdk/'judas_collision_cook','mesh',ROOT/'assets/models/beacon.obj',OUT/'beacon.judascollision','0b3ac0e5c8b04d1e9f7a2c6d5e4f3a21','0'])
    run('sdk_cook_mesh',[sdk/'judas_cook',ROOT/'assets/models/beacon.obj',OUT/'beacon.judasmesh','1024','3','0.03'])
    run('sdk_save_identity',[sdk/'judas_save','identity',project])
    tools=OUT/'tool-authoring-project';shutil.copytree(pristine,tools)
    (tools/'Sources').mkdir(exist_ok=True); volume=tools/'Sources/sdk-volume.judasvolume'
    run('sdk_volume_create',[sdk/'judas_volume_author','create',volume,'9','9','9','8','8','8'])
    run('sdk_volume_remove',[sdk/'judas_volume_author','brush',volume,'remove','0','0','0','2','1'])
    run('sdk_volume_validate',[sdk/'judas_volume_author','validate',volume])
    run('sdk_volume_cook',[sdk/'judas_volume_author','cook',tools/project.name,'Sources/sdk-volume.judasvolume'])
    # Existing editor automation exercises real authoring/undo/PlayStop and saves only to owned output.
    editor=OUT/'editor-project';shutil.copytree(pristine,editor)
    run('sdk_editor_play_stop',[sdk/'judas_editor',editor/project.name],extra={'JUDAS_EDITOR_AUTOTEST':str(OUT/'editor'), 'JUDAS_EDITOR_AUTOTEST_AUTHORING':'1'},timeout=180)
    run('sdk_export_stress_lab',[sdk/'judas_export',project,OUT/'stress-export',sdk/'judas'],timeout=180)
    moved=Path('/tmp/Judas-PreAlphaPass4-Physics-60a9217');assert not moved.exists();shutil.copytree(OUT/'stress-export',moved)
    script=OUT/'relocated.txt';script.write_text('FRAMES 90\nLOG_EVERY 0\nACTION watch 1 1 2\nSCREENSHOT 60 '+str(OUT/'relocated.png')+'\n')
    run('sdk_relocated_standalone',[moved/'judas'],cwd=Path('/tmp'),extra={'JUDAS_TEST_SCRIPT':str(script)},timeout=180)
    receipts={}
    for p in [sdk/'SDK_MANIFEST.json',sdk/'BUILD_INFO.json',sdk/'SHA256SUMS',
              OUT/'stress-export'/'BUILD_PROVENANCE.json',OUT/'stress-export'/'DEPENDENCIES.json',
              OUT/'stress-export'/'game.judasproj',OUT/'editor.saved.judas',OUT/'relocated.png']:
        if p.exists():receipts[str(p)]=dict(bytes=p.stat().st_size,sha256=digest(p))
    save(EV/'PACKAGE_RECEIPTS.json',receipts)
    for p in [sdk/'SDK_MANIFEST.json',OUT/'stress-export'/'BUILD_PROVENANCE.json',
              OUT/'stress-export'/'DEPENDENCIES.json',OUT/'editor.edit.png',OUT/'editor.play.png',OUT/'relocated.png']:
        if p.exists():shutil.copyfile(p, EV/p.name)
    log=(EV/'logs/sdk_editor_play_stop.log').read_text()
    save(EV/'EDITOR_ASSERTIONS.json', dict(
        sharedBatchUndo='shared authoring batch + undo PASS' in log,
        duplicateUndo='undo restores: IDENTICAL' in log,
        playStopRestoresAuthored='authored scene after play/stop is IDENTICAL' in log,
        save='[editor autotest] save ok' in log,
        noFailureToken='FAIL' not in log))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('stage',choices=['build','functional','performance','packaging']);args=parser.parse_args()
    EV.mkdir(exist_ok=True,parents=True);OUT.mkdir(exist_ok=True,parents=True)
    globals()[args.stage]()
