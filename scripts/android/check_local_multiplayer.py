#!/usr/bin/env python3
"""M89 native Android emulator proof. Uses ordinary APK, native multitouch and
kernel virtual controllers, screenshots and logcat; no game/native test backdoor.
Requires Pillow for inspecting actual moving colored bot pixels. NEVER target a
physical phone: this fixture uses root/uinput on an isolated 720x1280 API36 AVD.
"""
import argparse, subprocess, time, json, hashlib
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--adb',required=True);p.add_argument('--serial',default='emulator-5554');p.add_argument('--apk',type=Path,required=True);p.add_argument('--gamepads',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--touch-device',default='/dev/input/event2');a=p.parse_args()
if not a.serial.startswith('emulator-') or not a.touch_device.startswith('/dev/input/event'):p.error('Isolated emulator input only')
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);base=[a.adb,'-s',a.serial];package='org.projectjudas.corescramble';results=[]
def adb(*args,**kw):return subprocess.check_output(base+list(args),**kw)
def shell(*args):return adb('shell',*args,text=True).strip()
def check(ok,name,**details):
    results.append({'check':name,'pass':bool(ok),**details});(out/'results.json').write_text(json.dumps({'method':'Android16 x86_64 emulator; synthetic kernel input, not physical controllers/touch hardware','apkSha256':hashlib.sha256(a.apk.read_bytes()).hexdigest(),'checks':results},indent=2)+'\n');print(('PASS 'if ok else'FAIL ')+name,flush=True)
    if not ok:raise SystemExit(name)
def screen(name):
    f=out/(name+'.png');f.write_bytes(adb('exec-out','screencap','-p'));return f
# Isolate round state between input controls rather than relying on visual resets.
def launch():
    shell('am','force-stop',package);shell('am','start','-n',package+'/org.projectjudas.runtime.JudasActivity','--ez','judas.profile','true');time.sleep(1.5)
def tap(x,y):shell('input','tap',str(x),str(y));time.sleep(.25)
def mt(points,down=False,up=False):
    events=[]
    for slot,x,y in points:
        events.append((3,47,slot))
        if down:events.append((3,57,600+slot))
        if up:events.append((3,57,-1))
        else:events.extend([(3,53,round(x*32767)),(3,54,round(y*32767)),(3,58,900),(3,48,14)])
    events.append((0,0,0));shell('; '.join(f'sendevent {a.touch_device} {t} {c} {v}'for t,c,v in events))
def bots(path):
    im=Image.open(path).convert('RGB');w,h=im.size;pix=im.load();found=[]
    for which in range(2):
        mask=set()
        for y in range(int(h*.20),int(h*.86)):
            for x in range(w):
                r,g,b=pix[x,y]
                # This fixture identifies the authored bots' shaded highlights. Full
                # hue masks merge a bot touching its identically colored goal.
                if (30<r<110 and g>240 and b>245)if which==0 else(r>245 and 125<g<190 and 105<b<170):mask.add(y*w+x)
        blobs=[]
        while mask:
            stack=[mask.pop()];component=[]
            while stack:
                k=stack.pop();component.append(k)
                for n in (k-1,k+1,k-w,k+w):
                    if n in mask:mask.remove(n);stack.append(n)
            xs=[k%w for k in component];ys=[k//w for k in component];bw=max(xs)-min(xs)+1;bh=max(ys)-min(ys)+1
            if 20<len(component)<1800 and bw<65 and .6<bw/bh<1.8:blobs.append({'x':sum(xs)/len(xs),'y':sum(ys)/len(ys),'pixels':len(xs),'bounds':[min(xs),min(ys),bw,bh]})
        blobs.sort(key=lambda b:b['pixels'],reverse=True);found.append(blobs)
    (path.with_suffix('.blobs.json')).write_text(json.dumps(found,indent=2)+'\n')
    check(all(found),'actual cyan/coral physical bots identified in '+path.stem,blobs=found)
    return [b[0]for b in found]
check(shell('id','-u')=='0','isolated emulator root available')
check('720x1280'in shell('wm','size'),'fixture display dimensions')
check('virtio_input_multi_touch_1'in shell('getevent','-lp',a.touch_device),'native MT input device identified')
adb('install','-r',str(a.apk.resolve()));shell('logcat','-c');shell('wm','user-rotation','lock','0')
try:
    launch();before=bots(screen('single-before'));mt([(0,.20,.35)],True);time.sleep(.12);mt([(0,.36,.35)]);time.sleep(.8);mt([(0,0,0)],up=True);time.sleep(.2);after=bots(screen('single-after'));check(after[0]['x']-before[0]['x']>5 and abs(after[1]['x']-before[1]['x'])<4,'one native touch moves only its owning physical bot',before=before,after=after)
    launch();before=bots(screen('two-before'));mt([(0,.20,.35),(1,.30,.70)],True);time.sleep(.12);mt([(0,.36,.35),(1,.14,.70)]);time.sleep(.8);mt([(0,0,0),(1,0,0)],up=True);time.sleep(.2);after=bots(screen('two-after'));check(after[0]['x']-before[0]['x']>5 and before[1]['x']-after[1]['x']>5,'simultaneous native touches move two physical bots in opposite directions',before=before,after=after)
    tap(540,208);menu=screen('local-menu');r,g,b=Image.open(menu).convert('RGB').getpixel((200,310));check(g>70 and b>80 and r<120,'owner touch opens actual local menu with idle assigned mouse');tap(360,329);closed=screen('local-resumed');r,g,b=Image.open(closed).convert('RGB').getpixel((200,310));check(g<45 and b<60,'owner touch resumes its local menu')
    # Existing generic UI buttons: split views, then four independent seats.
    tap(630,1196);screen('split-two');tap(450,1196);screen('split-four');shell('wm','user-rotation','lock','1');time.sleep(1);landscape=screen('landscape-four');check(Image.open(landscape).width>Image.open(landscape).height,'rotation changes native viewport and game layout');shell('input','keyevent','3');time.sleep(.8);shell('am','start','-n',package+'/org.projectjudas.runtime.JudasActivity');time.sleep(.8);screen('resumed-four');check(bool(shell('pidof',package)),'app resumes with live native runtime')
    shell('wm','user-rotation','lock','0');launch();adb('push',str(a.gamepads.resolve()),'/data/local/tmp/m89-gamepads');shell('chmod','755','/data/local/tmp/m89-gamepads');process=subprocess.Popen(base+['shell','/data/local/tmp/m89-gamepads'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    pad_before=None;pad_after=None;markers=[]
    for line in process.stdout:
        marker=line.strip();markers.append(marker)
        if marker=='CREATED_TWO':time.sleep(2);pad_before=bots(screen('pads-before'))
        if marker=='DRIVING_OPPOSITE':time.sleep(1);pad_after=bots(screen('pads-driving'))
        if marker=='FIRST_DISCONNECTED':screen('pad-one-disconnected')
        if marker=='SECOND_STILL_DRIVING':screen('pad-two-still-driving')
    check(process.wait()==0 and markers[-1]=='REMOVED_TWO','two native Android gamepads hotplug/drive/remove through normal InputDevice adapter',markers=markers)
    check(pad_before and pad_after and pad_after[0]['x']-pad_before[0]['x']>5 and pad_before[1]['x']-pad_after[1]['x']>5,'distinct Android gamepads independently drive actual game bots',before=pad_before,after=pad_after)
    shell('input','keyevent','4');time.sleep(.25);screen('exit-menu');tap(360,775);time.sleep(.8)
    profile=adb('exec-out','run-as',package,'cat','files/judas-profile.json');(out/'profile.json').write_bytes(profile)
    log=shell('logcat','-d','-v','brief');(out/'logcat.txt').write_text(log)
    # Check the Judas process's explicit script/native failures, not unrelated OS noise.
    bad=[l for l in log.splitlines()if ('script asset='in l or 'Fatal signal'in l or 'FATAL EXCEPTION'in l or 'Graphics context lost'in l)]
    check(not bad,'no script fault/native crash/context loss',failures=bad)
finally:
    shell('wm','user-rotation','lock','0');shell('am','force-stop',package)
print('M89 emulator functional checks complete. Human/hardware acceptance remains pending.')
