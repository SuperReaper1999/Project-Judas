#!/usr/bin/env python3
"""Prepare/build NEW80f sampler-only driver against immutable accepted core.

Plan only unless --execute. No engine rebuild, source edits, or workloads.
The existing bounded, signal-safe sampler utility is copied exactly, not altered.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SAMPLER=ROOT/'.cache/physics-exhaustive-profile-20261021/sampler/MainThreadSampler.h'


def sha(path):
    value=hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b''):value.update(chunk)
    return value.hexdigest()


def replace_one(text,marker,replacement):
    if text.count(marker)!=1:raise RuntimeError('Expected one exact diagnostic insertion point: '+marker)
    return text.replace(marker,replacement)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',type=Path,default=HERE/'baseline')
    parser.add_argument('--output',type=Path,default=HERE/'sampled-build')
    parser.add_argument('--execute',action='store_true')
    parser.add_argument('--stage-capture',action='store_true',help='Also read completed existing-profiler frames, activated only by JUDAS_STAGE_CAPTURE')
    args=parser.parse_args()
    baseline,output=args.baseline.resolve(),args.output.resolve()
    if output.exists() or not output.is_relative_to(HERE):raise SystemExit('Fresh private output required')
    frozen=json.loads((baseline/'FROZEN.json').read_text())
    if frozen['accepted_head']!='80f7b2a5f96cdf1d56c5c88043c16edefe70a4c3':raise RuntimeError('Not NEW80f frozen baseline')
    core=baseline/'libjudas_engine.a'
    if sha(core)!=frozen['artifacts']['libjudas_engine.a']['sha256']:raise RuntimeError('Frozen core changed')
    driver=baseline/'source/tests/PhysicsStressLiveBenchmark.cpp'
    if sha(driver)!=frozen['sources']['tests/PhysicsStressLiveBenchmark.cpp']['sha256']:raise RuntimeError('Frozen driver changed')
    text=driver.read_text()
    text='#include "MainThreadSampler.h"\n#include "BuildIdentity.h"\n#include <glad/gl.h>\n#include <stdexcept>\n'+text
    text=replace_one(text,' frames<<std::setprecision(12);',
        ' const auto cpuPeriodUs=std::getenv("JUDAS_CPU_SAMPLE_PERIOD_US")?std::stoull(std::getenv("JUDAS_CPU_SAMPLE_PERIOD_US")):2000ull;\n'
        ' sampler::Session cpuSampler(std::filesystem::path(output)/"sampling",std::getenv("JUDAS_CPU_SAMPLE")!=nullptr,cpuPeriodUs);\n'
        ' frames<<std::setprecision(12);')
    text=replace_one(text,'steps.flush();});','steps.flush();cpuSampler.MarkCompletedStep(step);});')
    text=replace_one(text,'control.beforeShutdown=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){',
        'control.beforeShutdown=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){cpuSampler.Stop();')
    text=replace_one(text,'control.hostReady=[&](EngineHost& host){',
        'control.hostReady=[&](EngineHost& host){'
        'const auto* glVendor=glGetString(GL_VENDOR);const auto* glRenderer=glGetString(GL_RENDERER);'
        'const std::string vendor=glVendor?reinterpret_cast<const char*>(glVendor):"";'
        'const std::string renderer=glRenderer?reinterpret_cast<const char*>(glRenderer):"";'
        'std::cerr<<"NATIVE_GL vendor="<<vendor<<" renderer="<<renderer<<"\\n";'
        'if(vendor.find("NVIDIA")==std::string::npos||renderer.find("NVIDIA")==std::string::npos)'
        'throw std::runtime_error("native NVIDIA renderer required for matched host performance run");'
        'std::cerr<<"BUILD_IDENTITY "<<BuildIdentityJson()<<"\\n";')
    if args.stage_capture:
        text='#include "StageCapture.h"\n'+text
        text=replace_one(text,' frames<<std::setprecision(12);',
            ' if(std::getenv("JUDAS_STAGE_CAPTURE")&&!profile)throw std::runtime_error("stage capture requires existing profiler enabled");\n'
            ' stage_capture::Session stageCapture(output,std::getenv("JUDAS_STAGE_CAPTURE")!=nullptr);\n'
            ' frames<<std::setprecision(12);')
        text=replace_one(text,'control.beforeFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){',
            'control.beforeFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){stageCapture.AtFrameStart(frame);')
        text=replace_one(text,'cpuSampler.Stop();','cpuSampler.Stop();stageCapture.AtShutdown(frame);')
    # Source/header snapshots are frozen, and all current module dependencies
    # are fingerprinted. The engine core never comes from the mutable build.
    for name,row in frozen['sources'].items():
        if name.startswith(('src/','cmake/')) and (ROOT/name).is_file() and sha(ROOT/name)!=row['sha256']:
            raise RuntimeError('Source changed before sampled driver build: '+name)
    variables={}
    for line in (baseline/'recipes/benchmark-flags.make').read_text().splitlines():
        if ' = ' in line:
            key,value=line.split(' = ',1);variables[key]=shlex.split(value)
    source=output/'SampledLive.cpp';obj=output/'SampledLive.cpp.o';binary=output/'sampled-live'
    compile_argv=['/usr/bin/c++',*variables['CXX_DEFINES'],'-I'+str(output),*variables['CXX_INCLUDES'],
                  *variables['CXX_FLAGS'],'-g1','-c',str(source),'-o',str(obj)]
    link=shlex.split((baseline/'recipes/benchmark-link.txt').read_text())
    link=[token for token in link if not token.startswith('-Wl,--dependency-file=')]
    if sum(token.endswith('/tests/PhysicsStressLiveBenchmark.cpp.o') for token in link)!=1:raise RuntimeError('Unexpected benchmark driver recipe')
    if sum(token=='libjudas_engine.a' for token in link)!=1:raise RuntimeError('Unexpected core link recipe')
    link=[str(core) if token=='libjudas_engine.a' else str(obj) if token.endswith('/tests/PhysicsStressLiveBenchmark.cpp.o') else token for token in link]
    link[link.index('-o')+1]=str(binary)
    modules={str(core):sha(core)}
    for token in link:
        if token.endswith(('.a','.so')) or '.so.' in token:
            path=Path(token);path=path if path.is_absolute() else ROOT/'build'/path
            modules[str(path.resolve())]=sha(path)
    report={'schema':'judas.accepted80f7.sampler-only-build.v1','execute':args.execute,
            'baseline_manifest_sha256':sha(baseline/'FROZEN.json'),'core':str(core),'core_sha256':sha(core),
            'driver_original_sha256':sha(driver),'driver_modified_sha256':hashlib.sha256(text.encode()).hexdigest(),
            'sampler_original':str(SAMPLER),'sampler_sha256':sha(SAMPLER),
            'optional_stage_capture':args.stage_capture,
            'compile':compile_argv,'link':link,'cwd':str(ROOT/'build'),'modules':modules,
            'notes':['Only benchmark driver gains optional CPU sampling and native GL validation; no engine observer/overlay.',
                     'CLOCK_THREAD_CPUTIME_ID main-thread PC samples, 2ms default, max131072 samples; not hardware cycles or stacks.',
                     'last_completed_step labels are the most recent published fixed step, not exact in-flight step identity.',
                     'GetBuildIdentity is the actual public provenance API; no GetSourceBuildIdentity exists.',
                     'Stop() runs before GL/SDL unloading; maps/module layout are captured while modules remain loaded.']}
    if not args.execute:print(json.dumps(report,indent=2));return
    output.mkdir(parents=True);source.write_text(text);shutil.copy2(SAMPLER,output/'MainThreadSampler.h')
    if args.stage_capture:
        stage=HERE/'StageCapture.h';shutil.copy2(stage,output/'StageCapture.h');report['stage_capture_sha256']=sha(stage)
    (output/'BUILD.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,argv in [('compile',compile_argv),('link',link)]:
        with (output/(name+'.log')).open('w') as stream:
            status=subprocess.run(argv,cwd=ROOT/'build',stdout=stream,stderr=subprocess.STDOUT)
        report[name+'_exit']=status.returncode
        (output/'BUILD.json').write_text(json.dumps(report,indent=2)+'\n')
        if status.returncode:raise SystemExit(status.returncode)
    report.update(binary=str(binary),binary_sha256=sha(binary),finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  modules_unchanged=all(sha(Path(path))==value for path,value in modules.items()))
    (output/'BUILD.json').write_text(json.dumps(report,indent=2)+'\n')
    if not report['modules_unchanged']:raise SystemExit('Linked module changed while building')
    print(json.dumps({'binary':str(binary),'binary_sha256':report['binary_sha256'],'core_sha256':sha(core)},indent=2))


if __name__=='__main__':main()
