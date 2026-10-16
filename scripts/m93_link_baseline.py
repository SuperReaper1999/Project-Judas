#!/usr/bin/env python3
"""Link observer to the frozen M92 engine AND its matching image decoder."""
import subprocess,shlex,pathlib,json
r=pathlib.Path(__file__).resolve().parents[1];base=r/'.cache/m93/baseline';commands=[]
(base/'third_party').mkdir(exist_ok=True)
for f in ['stb_image.h']:
 p=base/'third_party'/f
 if not p.exists():p.symlink_to(r/'third_party'/f)
for name in ['TextureLoader','AppIcon']:
 cmd=['c++','-O3','-DNDEBUG','-std=c++17','-I'+str(base/'src'),'-I'+str(r/'build/generated'),'-I/usr/include/SDL2','-c',str(base/f'src/{name}.cpp'),'-o',str(base/f'{name}.o')];commands.append(cmd);subprocess.run(cmd,check=True)
cmd=['ar','rcs',str(base/'libjudas_app_icon.a'),str(base/'TextureLoader.o'),str(base/'AppIcon.o')];commands.append(cmd);subprocess.run(cmd,check=True)
link=shlex.split((r/'build/CMakeFiles/judas_render_camera_tests.dir/link.txt').read_text());deps=link[link.index('libjudas_engine.a')+1:];deps=[str((base/x)if x=='libjudas_app_icon.a' else(r/'build'/x))if x.endswith('.a')else x for x in deps]
cmd=['c++','-O3','-DNDEBUG','-std=c++17','-I'+str(base/'src'),'-I'+str(r/'third_party/glad/include'),'-I/usr/include/SDL2',str(r/'tests/AssetPipelineBenchmark.cpp'),str(base/'libjudas_engine.a'),*deps,'-o',str(base/'asset_benchmark_final')];commands.append(cmd);subprocess.run(cmd,check=True);(base/'FINAL_LINK_COMMANDS.json').write_text(json.dumps(commands,indent=2)+'\n')
