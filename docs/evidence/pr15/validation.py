from pathlib import Path
import subprocess,json
out=Path("docs/evidence/pr15");results=[]
checks=[("portal",["./build/judas_portal_camera_tests"]),("render_camera",["./build/judas_render_camera_tests","--output","/tmp/pr15-render-camera"]),("planet_world",["./build/judas_planet_world_tests"]),("origin_physics",["./build/judas_origin_physics_tests"]),("planet_streaming",["./build/judas_planet_streaming_tests"]),("planet_application",["./build/judas_planet_application_tests","projects/planetary_terrain_lab/planetary_terrain_lab.judasproj","/tmp/pr15-planet-frames"]),("api_drift",["node","scripts/check_judasjs_api.mjs","--typescript","/tmp/judas-m85-types/node_modules/typescript/lib/typescript.js"])]
for name,cmd in checks:
 with (out/(name+".log")).open("w") as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
 results.append({"check":name,"exit":r.returncode});print(name,r.returncode,flush=True)
(out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
raise SystemExit(int(any(v["exit"] for v in results)))
