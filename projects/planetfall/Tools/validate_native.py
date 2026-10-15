"""Focused, sequential native game checks. No production-suite expansion."""
from pathlib import Path
import os,subprocess,sys
P=Path(__file__).resolve().parents[1];ROOT=P.parents[1]
out=Path(sys.argv[1] if len(sys.argv)>1 else '/tmp/planetfall-final-native');out.mkdir(parents=True,exist_ok=True)
cases=[('save','earth'),('cold','earth'),('earth','earth'),('avatar','earth'),('moon','moon'),('mars','mars'),('derelict','derelict'),('combat','earth-frontier'),('pirates','moon'),('roof','earth-coast'),('flight','earth'),('flight-block','earth'),('stream','earth')]
for mode,scene in cases:
 log=out/(mode+'.log')
 with log.open('w') as f:
  result=subprocess.run([str(ROOT/'build/judas_planetfall_application_tests'),str(P/'Scenes'/f'{scene}.judas'),str(out/mode),mode],stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,JUDAS_PROFILE='1'))
 lines=log.read_text().splitlines()
 for line in lines:
  if line.startswith(('SUMMARY','PERF','FAIL','SCRIPT')):print(mode+': '+line,flush=True)
 if result.returncode:print('Stopped on failed scenario: '+str(log),flush=True);sys.exit(result.returncode)
print('All focused native scenarios passed. This is not a full campaign human playthrough.',flush=True)
