#!/usr/bin/env python3
from pathlib import Path
import subprocess,hashlib,shlex,json,datetime
O=Path(__file__).resolve().parent;R=O.parents[2];B=R/'build';BASE=O.parent/'candidate-fastpaths-final/libjudas_engine.a';C=O/'corrected';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
old=(R/'src/physics/WorldCCDSearch.cpp').read_text();new=(C/'WorldCCDSearch.cpp').read_text();assert new==old.replace('if (!depth || bound->hi <= 0)','if (!depth)')
receipt=json.loads((O/'CORRECTED_BUILD_RESULTS.json').read_text());compile=receipt['build'][0];obj=C/'WorldCCDSearch.cpp.o';assert sha(obj)==compile['object_sha256']and sha(C/'WorldCCDSearch.cpp')==compile['source_sha256']
deps=shlex.split(Path(str(obj)+'.d').read_text().replace('\\\n',' ').split(':',1)[1]);compile['inputs']={p:sha(Path(p))for p in deps if Path(p).is_file()}
recipe=shlex.split((B/'CMakeFiles/judas_engine.dir/link.txt').read_text().splitlines()[0]);ordered=[p for p in recipe if p.endswith('.o')];names=subprocess.check_output(['ar','t',BASE],text=True).splitlines();assert [Path(p).name for p in ordered]==names
raw=BASE.read_bytes();assert raw[:8]==b'!<arch>\n';pos=8;long=b'';members=[]
while pos<len(raw):
 h=raw[pos:pos+60];assert h[58:60]==b'`\n';size=int(h[48:58]);name=h[:16].decode().strip();data=raw[pos+60:pos+60+size];pos+=60+size+(size&1)
 if name=='//':long=data;continue
 if name in ['/','/SYM64/']:continue
 if name.startswith('/')and name[1:].isdigit():name=long[int(name[1:]):].split(b'/\n',1)[0].decode()
 else:name=name.rstrip('/')
 members.append((name,data))
assert [p[0]for p in members]==names
inputs=[];mapping=[]
for ordinal,(source,(name,data))in enumerate(zip(ordered,members)):
 replace=source.endswith('.dir/src/physics/WorldCCDSearch.cpp.o');chosen=obj if replace else C/'members'/str(ordinal)/name
 if not replace:chosen.parent.mkdir(parents=True,exist_ok=True);chosen.write_bytes(data)
 inputs.append(str(chosen));mapping.append({'ordinal':ordinal,'recipe':source,'member':name,'baseline_sha256':hashlib.sha256(data).hexdigest(),'chosen_sha256':sha(chosen),'replaced':replace})
assert sum(row['replaced']for row in mapping)==1
archive=C/'libjudas_engine.a'
if archive.exists():archive.unlink()
subprocess.run(['ar','qc',str(archive),*inputs],check=True);subprocess.run(['ar','s',str(archive)],check=True)
cmd=shlex.split((B/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());cmd=[p for p in cmd if not p.startswith('-Wl,--dependency-file=')];cmd=[str(archive)if p=='libjudas_engine.a'else p for p in cmd];binary=C/'judas_physics_stress_live_benchmark';cmd[cmd.index('-o')+1]=str(binary)
with(C/'live-link.log').open('w')as log:result=subprocess.run(cmd,cwd=B,stdout=log,stderr=subprocess.STDOUT)
if result.returncode:print((C/'live-link.log').read_text());raise SystemExit(result.returncode)
# Preserve genuine absent-mechanism detection, independently of expected-failure reporting.
with(O/'original-detecting-failure.log').open('w')as log:negative=subprocess.run([str(O/'support-completeness-original')],stdout=log,stderr=subprocess.STDOUT)
assert negative.returncode==1
out={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'base_archive':str(BASE),'base_archive_sha256':sha(BASE),'compilation':compile,'archive_members':mapping,'archive_sha256':sha(archive),'normal':{'binary':str(binary),'binary_sha256':sha(binary),'link':cmd,'exit':result.returncode},'no_layout_change':True,'actual_change':'SupportContinuationSafe subdivision does not prune on nonpositive upper endpoint of a worst-extent lower-bound proxy; depth3/slop/native trajectories unchanged','original_detecting_regression':{'argv':[str(O/'support-completeness-original')],'exit':negative.returncode,'expected':1,'log_sha256':sha(O/'original-detecting-failure.log')}}
(C/'LIVE_BUILD.json').write_text(json.dumps(out,indent=2)+'\n');print(binary)
