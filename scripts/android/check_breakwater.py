#!/usr/bin/env python3
"""M88 native-emulator cold-save check. Uses the ordinary 720x1280 Breakwater
UI and a non-default real hit/lost-life state. Resets only this fixture app.
Hardware acceptance is separate. Install the matching APK before running.
"""
import argparse,subprocess,time,json,sys,hashlib
from pathlib import Path
from inspect_save import inspect
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--adb',required=True);p.add_argument('--serial',default='emulator-5554');p.add_argument('--apk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);opts=p.parse_args()
if not opts.serial.startswith('emulator-'):p.error('Use an isolated emulator')
adb=opts.adb;out=opts.output;out.mkdir(parents=True,exist_ok=True);base=[adb,'-s',opts.serial];package='org.projectjudas.breakwater';slot='files/judas/games/d94c72b149fa28880eab950953ef2151f7937cb7f2066defed0da2e7e1cb7267/Saves/Slots/breakwater.save'
def shell(*args):return subprocess.check_output(base+['shell',*args],text=True).strip()
def tap(x,y,wait=.25):shell('input','tap',str(x),str(y));time.sleep(wait)
def pull(name):
 p=out/name
 with p.open('wb')as f:subprocess.run(base+['exec-out','run-as',package,'cat',slot],stdout=f,check=True)
 return inspect(p)
def game(s):return next(v['state']for v in s['scripts']if v['entity']=='50')
assert '720x1280' in shell('wm','size'), 'Set the isolated AVD to 720x1280 for this UI fixture'
shell('am','force-stop',package);shell('am','start','-n',package+'/org.projectjudas.runtime.JudasActivity');time.sleep(1)
tap(600,1220,4)
# Pause and save the actual stable ready state after the first normal lost ball.
tap(650,55);tap(360,810,.5);before=pull('android-cold-before.save')
assert game(before)['phase']=='ready',game(before)
assert game(before)['hits']>0 and game(before)['lives']<3,game(before)
pid=shell('pidof',package)
shell('am','force-stop',package);shell('am','start','-n',package+'/org.projectjudas.runtime.JudasActivity');time.sleep(1)
# New process begins the original authored round; now restore the non-default save.
tap(650,55);tap(360,900,.5)
with (out/'android-cold-ready.png').open('wb')as f:subprocess.run(base+['exec-out','screencap','-p'],stdout=f,check=True)
tap(650,55);tap(360,810,.5);after=pull('android-cold-after.save')
a,b=game(before),game(after)
for key in ['score','lives','hits','powers','phase','wideUntil']:assert a[key]==b[key],(key,a,b)
assert before['entities']['taggedEntityIds']==after['entities']['taggedEntityIds']
assert before['entities']['bodies']==after['entities']['bodies']
assert pid!=shell('pidof',package)
result={'apkSha256':hashlib.sha256(opts.apk.read_bytes()).hexdigest(),'method':'Native Android 16 x86_64, real input hit/loss, M61 private save; force-stop/fresh process, load then re-save; independent participant decoding','before':before,'after':after,'differentProcess':True,'sameGameplayState':True,'samePhysicalEntityIdsAndBodyCounts':True,'physicalAcceptance':False}
(out/'android-cold-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'gameBefore':a,'gameAfter':b,'bodies':after['entities']['bodies'],'tagCounts':{k:len(v)for k,v in after['entities']['taggedEntityIds'].items()},'pass':True},indent=2))
shell('am','force-stop',package)
