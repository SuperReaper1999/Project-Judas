#!/usr/bin/env python3
"""Focused M85 receipts. No automatic downloads, history edits or promotions."""
import argparse,hashlib,json,pathlib,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
FOCUSED=['judas_planet_terrain_tests','judas_planet_world_tests','judas_planet_streaming_tests','judas_origin_physics_tests']
REGRESSIONS=['judas_character_motor_tests','judas_gravity_selection_tests','judas_world_coordinates_tests','judas_joint_tests','judas_kinematic_tests','judas_physics_cast_tests','judas_rigid_contact_tests','judas_collision_geometry_tests','judas_streaming_tests','judas_save_storage_tests','judas_terrain_authoring_tests','judas_volume_terrain_tests']
def main():
 p=argparse.ArgumentParser(__doc__);p.add_argument('--build-dir',default='build');p.add_argument('--output',required=True);p.add_argument('--graphics',action='store_true');p.add_argument('--regressions',action='store_true');p.add_argument('--typescript');p.add_argument('--fingerprints-only',action='store_true');a=p.parse_args();out=pathlib.Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True);build=(ROOT/a.build_dir).resolve();results=[]
 def run(name,command):
  with open(out/(name+'.log'),'w') as f:r=subprocess.run([str(v) for v in command],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT)
  results.append({'check':name,'exit':r.returncode});print(name,r.returncode,flush=True)
 if not a.fingerprints_only:
  for name in FOCUSED+(REGRESSIONS if a.regressions else []):run(name,[build/name])
  if a.graphics:run('planet_application',[build/'judas_planet_application_tests',ROOT/'projects/planetary_terrain_lab/planetary_terrain_lab.judasproj',out/'frames'])
  if a.typescript:run('api_drift',['node',ROOT/'scripts/check_judasjs_api.mjs','--typescript',a.typescript])
  (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 # Freeze exact sources from the M85 starting checkpoint, including newly
 # registered assets, not unrelated historical untracked experiments.
 base='e421645b3c310508ff618fe1e880a1b8cdbc019b';changed=subprocess.check_output(['git','diff','--name-only',base],cwd=ROOT,text=True).splitlines()
 new=['src/PlanetTerrain.h','src/PlanetTerrain.cpp','src/PlanetTiles.h','src/PlanetTiles.cpp','src/WorldOrigin.cpp','src/WorldPlanets.cpp','tests/PlanetTerrainTests.cpp','tests/PlanetWorldTests.cpp','tests/PlanetStreamingTests.cpp','tests/OriginPhysicsTests.cpp','tests/PlanetApplicationTests.cpp','scripts/m85_validation.py','docs/M85_PLANETARY_TERRAIN.md','docs/judasjs/planetary-terrain.md']
 project=ROOT/'projects/planetary_terrain_lab';new += [str(f.relative_to(ROOT)) for f in project.rglob('*') if f.is_file() and 'Saves' not in f.parts and '.cache' not in f.parts]
 sources=sorted({s for s in changed+new if not s.startswith('docs/evidence/') and (ROOT/s).is_file()})
 sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
 source_hashes={s:sha(ROOT/s) for s in sources}
 binaries={n:sha(build/n) for n in FOCUSED+REGRESSIONS+['judas_planet_application_tests','judas','judas_editor','judas_export'] if (build/n).is_file()}
 record={'startingCheckpoint':base,'capturedHEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'sources':source_hashes,'binaries':binaries}
 (out/'fingerprints.json').write_text(json.dumps(record,indent=2)+'\n');(out/'changed-files.txt').write_text('\n'.join(sources)+'\n')
 return int(any(r['exit'] for r in results))
if __name__=='__main__':sys.exit(main())
