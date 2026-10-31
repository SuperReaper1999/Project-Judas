import os,sys,time,subprocess,signal,json
from pathlib import Path
cg=Path('/sys/fs/cgroup'+Path('/proc/self/cgroup').read_text().strip().split('::')[1])
rows=[]
p=subprocess.Popen(sys.argv[2:],start_new_session=True)
try:
 while p.poll() is None:
  amount=int((cg/'memory.current').read_text());rows.append({'time':time.time(),'bytes':amount})
  if amount>2200*1024*1024:
   print('EARLY STOP: isolated test exceeded 2200 MiB',flush=True)
   os.killpg(p.pid,signal.SIGTERM);break
  time.sleep(.1)
 p.wait(timeout=10)
finally:
 if p.poll() is None:os.killpg(p.pid,signal.SIGKILL)
 Path(sys.argv[1]).write_text(json.dumps(rows))
sys.exit(p.returncode or 0)
