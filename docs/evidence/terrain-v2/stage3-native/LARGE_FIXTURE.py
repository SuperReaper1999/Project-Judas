# Reproduce the larger author/cook/export/play workflow. Run from repository root.
# Pass a fresh output directory; no existing files or consumer projects are changed.
from pathlib import Path
import sys, shutil, json, math
root = Path(sys.argv[1]); assert not root.exists()
shutil.copytree('projects/terrain_v2_workshop', root, ignore=shutil.ignore_patterns('Saves'))
f = root/'Authoring/highlands.judasterrain2'
s = json.loads(f.read_text()); s.update(samplesX=513,samplesZ=513,spacing=2,bottom=-128,top=256)
s['heights']=[]; s['weights']=[]
for z in range(513):
 for x in range(513):
  px=(x-256)*2; pz=(z-256)*2
  h=.65+7*math.exp(-pz*pz/36-px*px/190)+35*math.exp(-((px+180)**2+(pz+130)**2)/15000)+52*math.exp(-((px-170)**2+(pz+280)**2)/19000)
  s['heights'].append(h); rock=max(0,min(.8,(h-4)/55)); s['weights'].append([.85-rock,.15,rock,0])
f.write_text(json.dumps(s,separators=(',',':'))+'\n')
scene=root/'Scenes/highlands.judas'
scene.write_text(scene.read_text().replace('render.half-extents 24 8 24','render.half-extents 512 128 512').replace('body.half-extents 24 8 24','body.half-extents 512 128 512').replace('position 0 1.9 13','position 0 4 30'))
(root/'play.harness').write_text('FRAMES 300\nFRAME_SECONDS 0.016666667\nLOG_EVERY 60\nAXIS move_y 1 100 220\nSCREENSHOT 250 '+str(root/'large-play.png')+'\n')
print(root)
# build/judas_terrain_author v2-native ROOT/Authoring/highlands.judasterrain2 ROOT/Assets/terrain/highlands.judasmodel
# build/judas_export ROOT/terrain_v2_workshop.judasproj /tmp/new-package build/judas
# Rename the package, run its judas/game.judasproj from an unrelated working directory,
# with JUDAS_TEST_SCRIPT=ROOT/play.harness and JUDAS_ENGINE_ROOT=PACKAGE/engine.
