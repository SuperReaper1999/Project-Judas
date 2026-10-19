from pathlib import Path
import importlib.util,json,shutil
spec=importlib.util.spec_from_file_location('qualification',Path(__file__).with_name('validate.py'))
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
r.ROWS.extend(json.loads((r.OUT/'CHECKS.json').read_text()))
project=r.OUT/'editor-native-project';shutil.copytree(r.ROOT/'projects/character_demo',project)
assert r.run('editor-native-play-stop',[r.BUILD/'judas_editor',project/'character_demo.judasproj'],env={'DISPLAY':':0','WAYLAND_DISPLAY':'wayland-0','XDG_RUNTIME_DIR':'/run/user/1000','SDL_VIDEODRIVER':'x11','LIBGL_ALWAYS_SOFTWARE':'0','JUDAS_EDITOR_AUTOTEST':str(r.OUT/'editor-native'),'JUDAS_EDITOR_AUTOTEST_AUTHORING':'1'},timeout=180)
text=(r.OUT/'editor-native-play-stop.log').read_text()
for token in ['shared authoring batch + undo PASS','undo restores: IDENTICAL','authored scene after play/stop is IDENTICAL','save ok']:
 assert token in text,token
assert 'Mouse capture failed:' not in text
