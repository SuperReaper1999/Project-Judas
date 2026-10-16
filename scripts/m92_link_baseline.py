#!/usr/bin/env python3
"""Link current observers to frozen baseline headers/archive after baseline build. Run from repository root."""
import subprocess,pathlib,shlex,json
r=pathlib.Path.cwd();build=r/'build';base=r/'.cache/m92/baseline';link=shlex.split((build/'CMakeFiles/judas_render_camera_tests.dir/link.txt').read_text());deps=link[link.index('libjudas_engine.a')+1:];deps=[str(build/x) if x.endswith('.a') else x for x in deps]
for name,source in [('rendering_scale_cpu','RenderingScaleTests.cpp'),('shadow_scale_cpu','RenderingShadowScaleTests.cpp')]:
 cmd=['c++','-O3','-DNDEBUG','-std=c++17','-I'+str(base/'source/src'),'-I'+str(r/'third_party/glad/include'),'-I/usr/include/SDL2',str(r/'tests'/source),str(base/'libjudas_engine.a'),*deps,'-o',str(base/name)];(base/(name+'-link.json')).write_text(json.dumps(cmd,indent=2));subprocess.run(cmd,check=True)
