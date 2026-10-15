#!/usr/bin/env python3
"""Bounded M88 emulator profile, using the ordinary installed project and M56.
Ready mode compares identical renderer workloads. Active mode sends real logical
launch/drag input. This is not physical-phone performance acceptance.
"""
import argparse,json,subprocess,time,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--adb',required=True);p.add_argument('--serial',default='emulator-5554');p.add_argument('--shadows',choices=['off','on'],default='off');p.add_argument('--mode',choices=['ready','active'],default='active');p.add_argument('--seconds',type=float,default=4);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
if not a.serial.startswith('emulator-') or not 2<=a.seconds<=30:p.error('Use an isolated emulator and 2..30 seconds')
base=[a.adb,'-s',a.serial]
def shell(*args):return subprocess.check_output(base+['shell',*args],text=True).strip()
def tap(x,y):shell('input','tap',str(x),str(y))
def pull(remote,destination):
 with destination.open('wb')as f:subprocess.run(base+['exec-out','run-as','org.projectjudas.breakwater','cat',remote],stdout=f,check=True)
a.output.mkdir(parents=True,exist_ok=True)
shell('am','force-stop','org.projectjudas.breakwater')
shell('am','start','-n','org.projectjudas.breakwater/org.projectjudas.runtime.JudasActivity','--ez','judas.profile','true','--ez','judas.shadows','true'if a.shadows=='on'else'false')
time.sleep(1.2)
# A fresh process starts a new, identical authored round. Warm the renderer before
# the active run; no object mass, solver quality, cadence or game rule is changed.
if a.mode=='ready':time.sleep(a.seconds)
else:
 tap(600,1220);time.sleep(.7);shell('input','swipe','360','1000','575','1000','400');time.sleep(max(1,a.seconds-1.1))
memory=shell('dumpsys','meminfo','org.projectjudas.breakwater')
shell('input','keyevent','4');time.sleep(.35);tap(360,990);time.sleep(.7)
pull('files/judas-profile.json',a.output/'profile.json')
j=json.loads((a.output/'profile.json').read_text());frames=[f for f in j['frames']if not f['startup']and not f['fixed']['paused']]
def stats(values):
 v=sorted(values);return {'samples':len(v),'medianMs':statistics.median(v),'p95Ms':v[min(len(v)-1,int(.95*len(v)))],'worstMs':max(v)}if v else{}
result={'method':'Native Android 16 emulator, host GL, 720x1280; M56 enabled; no physical-device claim','mode':a.mode,'shadows':a.shadows,'frame':stats([f['interval_ns']/1e6 for f in frames if f['interval_ns']]),'fixed':stats([(s['end_ns']-s['start_ns'])/1e6 for f in frames for s in f['fixed']['steps']]),'capFrames':sum(f['fixed']['cap_reached']for f in frames),'discardedSimulationSeconds':sum(f['fixed']['discarded_seconds']for f in frames),'diagnostics':j['diagnostics'],'memory':memory}
(a.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k!='memory'},indent=2))
