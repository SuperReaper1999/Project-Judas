#!/usr/bin/python3
"""Fresh final native runtime/export only; preceding FLU platform receipt preserved."""
import pathlib,sys
p=pathlib.Path('docs/evidence/m78-continuation/probes/windows_execute_focused_matrix.py')
s=p.read_text();ns={};exec(compile(s[:s.index('try:\n # The manually completed')],str(p),'exec'),ns)
try:
 for case in ['runtime','export']:
  print('BEGIN '+case,flush=True);ns['op']('case',case);ns['result'](case)
  if case=='export':ns['cleanup'](case)
 print('Fresh native runtime/export complete; runtime retained for moved-package proof.',flush=True)
except Exception as e:
 ns['collect']();print('STOP '+str(e),flush=True);sys.exit(1)
