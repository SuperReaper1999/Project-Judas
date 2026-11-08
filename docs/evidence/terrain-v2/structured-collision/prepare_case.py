from pathlib import Path
import argparse,json,math,shutil,subprocess
root=next(p for p in Path(__file__).resolve().parents if (p/'CMakeLists.txt').is_file())
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=root/'.cache/terrain-v2/residency/kilometre-editable')
args=parser.parse_args()
out=args.output.resolve()
if out.exists():raise SystemExit('Keep the previous benchmark fixture; use a fresh output.')
shutil.copytree(root/'projects/terrain_v2_workshop',out,ignore=shutil.ignore_patterns('Saves','*.lock'))
n=1025;spacing=1
heights=[];weights=[]
for z in range(n):
 for x in range(n):
  px=(x-(n-1)/2)*spacing;pz=(z-(n-1)/2)*spacing
  h=.65+7*math.exp(-pz*pz/36-px*px/190)+4.2*math.exp(-((px+15)**2+(pz+13)**2)/52)+.16*math.sin(px*.35)*math.sin(pz*.29)
  rock=min(.85,max(0,(h-3.4)/5));earth=.13+.15*math.sin(px*.21+pz*.16)**2
  earth=round(earth*10000);rock=round(rock*10000)
  heights.append(round(h,4));weights.append([(10000-earth-rock)/10000,earth/10000,rock/10000,0])
source=json.loads((out/'Authoring/highlands.judasterrain2').read_text());source.update(samplesX=n,samplesZ=n,spacing=spacing,chunkCells=32,heights=heights,weights=weights)
file=out/'Authoring/highlands.judasterrain2';file.write_text(json.dumps(source,separators=(',',':'))+'\n')
print('source text bytes',file.stat().st_size,flush=True)
subprocess.run([str(root/'build/judas_terrain_author'),'v2-native',str(file),str(out/'Assets/terrain/highlands.judasmodel')],check=True)
print(out)
