from pathlib import Path
import argparse,os,subprocess,json,shutil,hashlib
root=Path(__file__).resolve().parents[3];parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);parser.add_argument('--project',type=Path,default=root/'projects/world_workshop');args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
env=dict(os.environ,SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(out/'user-data'),XDG_CONFIG_HOME=str(out/'config'));cli=root/'build/judas_scene_author';records=[]
def run(label,argv,extra=None):
 p=subprocess.run([str(v) for v in argv],cwd=root,env=dict(env,**(extra or {})),capture_output=True,text=True,timeout=180);(out/(label+'.log')).write_text(p.stdout+p.stderr);records.append({'name':label,'exit':p.returncode,'command':[str(v) for v in argv]});print(label,p.returncode,flush=True);assert p.returncode==0,p.stderr[-2000:]
base=args.project.resolve()
for label in ['a','b']:
 copy=out/label;shutil.copytree(base,copy);recipe=copy/'Authoring/quarter.judasrecipe';doc=json.loads(recipe.read_text());doc['data']['profile'][0][0]=-3.5;recipe.write_text(json.dumps(doc));scene=copy/'Scenes/workshop.judas';project=copy/'world_workshop.judasproj';run('generate-'+label,[cli,'--generate',recipe,scene,project,scene,'--overwrite'])
assert (out/'a/Scenes/workshop.judas').read_bytes()==(out/'b/Scenes/workshop.judas').read_bytes()
def products(path):return {str(p.relative_to(path)):hashlib.sha256(p.read_bytes()).hexdigest() for p in path.rglob('*') if p.is_file()}
assert products(out/'a/Assets/Generated')==products(out/'b/Assets/Generated')
run('editor',[root/'build/judas_editor',out/'a/world_workshop.judasproj'],{'JUDAS_EDITOR_AUTOTEST':str(out/'editor'),'JUDAS_EDITOR_AUTOTEST_AUTHORING':'1','JUDAS_EDITOR_AUTOTEST_AUTHORING_LOCALE':'ar'})
log=(out/'editor.log').read_text();assert 'shared authoring batch + undo PASS' in log and 'authored scene after play/stop is IDENTICAL' in log and 'save ok' in log
run('performance',[root/'build/judas_world_authoring_performance',out/'performance'])
run('export-edited-final',[cli,'--build-project',out/'a/world_workshop.judasproj',out/'package',root/'build/judas',root])
steps=out/'steps.txt';steps.write_text('FRAMES 300\nLOG_EVERY 0\nACTION pause 1 170 171\nEXPECT_PAUSED 1 180\nSCREENSHOT 190 '+str(out/'paused.png')+'\nACTION pause 1 210 211\nEXPECT_PAUSED 0 220\nSCREENSHOT 250 '+str(out/'edited-runtime.png')+'\n')
run('edited-runtime',[out/'package/judas'],{'JUDAS_TEST_SCRIPT':str(steps)})
(out/'RESULTS.json').write_text(json.dumps({'pass':True,'deterministic_scene_and_cooked_products':True,'commands':records,'note':'Affected offline authoring/editor and workshop script checks only. Engine runtime unchanged since full gate.'},indent=2)+'\n')
