"""Original short combat cues synthesized locally; no recordings/libraries.

Writes mono 44.1 kHz 16-bit WAV assets. Gameplay and mixer levels live in JS/content.
"""
from pathlib import Path
import math, random, struct, wave, hashlib, json
P=Path(__file__).resolve().parents[1];RATE=44100

def cue(name,duration,render):
 random.seed('Lastlight/'+name);values=[];noise=0.0
 for n in range(int(duration*RATE)):
  t=n/RATE;white=random.uniform(-1,1);noise=.8*noise+.2*white
  values.append(render(t,white,noise))
 peak=max(.001,max(abs(x) for x in values));values=[int(max(-1,min(1,x/peak*.77))*32767) for x in values]
 path=P/'Assets/audio'/f'{name}.wav'
 with wave.open(str(path),'wb') as f:f.setparams((1,2,RATE,len(values),'NONE','not compressed'));f.writeframes(struct.pack('<'+'h'*len(values),*values))
 id=hashlib.md5(('lastlight/'+str(path.relative_to(P))).encode()).hexdigest()
 path.with_name(path.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{id}"\ntype audio\nsource ""\n')
 return {'name':name,'id':id,'path':str(path.relative_to(P))}
def pulse(t,start,decay):return math.exp(-(t-start)*decay) if t>=start else 0
records=[]
records.append(cue('rifle_fire',.24,lambda t,w,n:(w*.75+n*.3)*math.exp(-t*65)+.45*math.sin(2*math.pi*(135*t-65*t*t))*math.exp(-t*25)+w*.2*pulse(t,.07,95)))
records.append(cue('reload_click',.20,lambda t,w,n:sum((w*.65+math.sin(2*math.pi*2200*t)*.25)*pulse(t,s,140) for s in [0,.035,.115])*.65))
records.append(cue('melee_thud',.20,lambda t,w,n:(.5*n+.65*math.sin(2*math.pi*(92*t-120*t*t)))*math.exp(-t*27)+w*.16*math.exp(-t*75)))
records.append(cue('empty_click',.075,lambda t,w,n:(.5*w+.4*math.sin(2*math.pi*2800*t))*math.exp(-t*125)))
records.append(cue('explosion',.85,lambda t,w,n:(.7*n+.35*w*math.exp(-t*12)+.4*math.sin(2*math.pi*(70*t-25*t*t)))*math.exp(-t*5.5)))
(P/'Tools/combat-audio.json').write_text(json.dumps({'assets':records,'provenance':'Original deterministic synthesis; no external recordings/samples','format':'Mono PCM 16-bit / 44100 Hz'},indent=2)+'\n')
print(json.dumps(records,indent=2))
