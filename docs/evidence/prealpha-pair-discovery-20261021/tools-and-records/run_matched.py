from pathlib import Path
import subprocess,json,time
here=Path(__file__).resolve().parent;root=here.parents[1]
cases=[('baseline1800','baseline','stationary','1800'),('combined1800','combined','stationary','1800'),('baseline-stationary-live35','baseline','stationary','live35'),('combined-stationary-live35','combined','stationary','live35'),('baseline-wasd-live35-v3','baseline','wasd','live35'),('combined-wasd-live35','combined','wasd','live35')]
report=[]
for label,which,camera,mode in cases:
 binary=here/('diagnostic-build' if which=='baseline' else 'combined')/'live'
 core=here/'baseline/libjudas_engine.a' if which=='baseline' else root/'build/libjudas_engine.a'
 argv=['python3',str(here/'run_current.py'),'--binary',str(binary),'--core',str(core),'--output',str(here/label),'--mode',mode,'--camera',camera,'--extra-source',str(here/'diagnostic-build/DiagnosticLive.cpp'),'--execute']
 with (here/(label+'.log')).open('w') as log:code=subprocess.run(argv,cwd=root,stdout=log,stderr=subprocess.STDOUT).returncode
 report.append({'name':label,'exit':code,'argv':argv});(here/'MATCHED_RUNS.json').write_text(json.dumps(report,indent=2)+'\n');print(label,code,flush=True)
 if code:raise SystemExit(code)
