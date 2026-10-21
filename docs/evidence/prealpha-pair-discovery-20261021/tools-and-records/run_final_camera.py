from pathlib import Path
import subprocess,json
here=Path(__file__).resolve().parent;root=here.parents[1];rows=[]
for label,which,profile in [('baseline-wasd-final','baseline',False),('combined-wasd-final','combined',False),('combined-profile900','combined',True)]:
 core=here/'baseline/libjudas_engine.a' if which=='baseline' else root/'build/libjudas_engine.a'
 args=['python3',str(here/'run_current.py'),'--binary',str(here/which/'live-v4'),'--core',str(core),'--output',str(here/label),'--mode','900' if profile else 'live35','--camera','stationary' if profile else 'wasd','--extra-source',str(here/'diagnostic-build/DiagnosticLive-v4.cpp'),'--execute']
 if profile:args.extend(['--profile','--stage-capture'])
 with (here/(label+'.log')).open('w') as log:code=subprocess.run(args,cwd=root,stdout=log,stderr=subprocess.STDOUT).returncode
 rows.append({'name':label,'argv':args,'exit':code});(here/'FINAL_CAMERA_RUNS.json').write_text(json.dumps(rows,indent=2)+'\n');print(label,code,flush=True)
 if code:raise SystemExit(code)
