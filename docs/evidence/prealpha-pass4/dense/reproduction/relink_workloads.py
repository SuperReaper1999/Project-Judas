#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,shlex,hashlib,json,datetime
p=argparse.ArgumentParser();p.add_argument('--archive',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path.cwd();o=a.output.resolve();o.mkdir(exist_ok=True);archive=a.archive.resolve();records=[]
for name in ['judas_physics_stress_live_benchmark','judas_physics_scaling_diagnostics']:
 args=shlex.split((r/'build/CMakeFiles'/f'{name}.dir/link.txt').read_text());objs=[str((r/'build'/x).resolve())for x in args if x.endswith('.o')];libs=args[args.index('libjudas_engine.a')+1:];libs=[str((r/'build'/x).resolve())if x.endswith('.a')and not x.startswith('/')else x for x in libs];binary=o/name;cmd=['/usr/bin/c++','-O3','-DNDEBUG',*objs,'-o',str(binary),str(archive),*libs]
 with(o/(name+'.link.log')).open('w')as f:subprocess.run(cmd,cwd=r/'build',stdout=f,stderr=subprocess.STDOUT,check=True)
 records.append({'argv':cmd,'archiveSha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'binarySha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'objects':{x:hashlib.sha256(Path(x).read_bytes()).hexdigest()for x in objs}})
(o/'WORKLOAD_BUILDS.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'records':records},indent=2)+'\n')
