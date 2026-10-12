#!/usr/bin/env python3
import pathlib,hashlib,json,shutil
r=pathlib.Path.cwd();base=r/'.cache/m78-cold-boundary/row-journal';out=r/'.cache/m78-cold-boundary/row-journal-warm';out.mkdir(exist_ok=False)
s=(base/'JointSolver.cpp').read_text();s=s.replace('bool bjActive=false;','bool bjActive=false,bjDiscardFirstWarm=false;')
old='bjTick=-1;const char* n=std::getenv("JUDAS_BOUNDARY_STEPS");';assert s.count(old)==1
s=s.replace(old,'bjTick=-1;const char* discard=std::getenv("JUDAS_BOUNDARY_DISCARD_FIRST_JOINT_SEEDS");bjDiscardFirstWarm=discard&&std::string(discard)=="1";const char* n=std::getenv("JUDAS_BOUNDARY_STEPS");')
old='void JointSolver::Prepare(const std::vector<JointInput>& inputs,float dt,bool warm,bool impact){';assert s.count(old)==1
s=s.replace(old,old+'\n if(bjActive&&bjTick==0&&bjPrepare==0&&!impact&&bjDiscardFirstWarm)warm=false;')
(out/'JointSolver.cpp').write_text(s);shutil.copyfile(base/'BalanceLabBoundaryObserver.cpp',out/'BalanceLabBoundaryObserver.cpp')
d=json.loads((base/'IDENTITY.json').read_text());d['scope']='Passive journal by default. Optional explicitly isolated single FIRST joint warm-SEED disable iff JUDAS_BOUNDARY_DISCARD_FIRST_JOINT_SEEDS=1. No contactgeometry/cache/commands or clocks touched; initial contact rows are independently established zero. Original dt60Hz, gains, thresholds and scripts unchanged.';d['overlaySHA256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in out.glob('*.cpp')};(out/'IDENTITY.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d['overlaySHA256'],indent=2))
