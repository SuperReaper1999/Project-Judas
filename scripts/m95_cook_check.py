from pathlib import Path
import shutil,subprocess,json,hashlib,argparse
R=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser();parser.add_argument('--output',default='.cache/m95/cook-check');args=parser.parse_args();out=R/args.output;out.mkdir(parents=True,exist_ok=False)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
results={}
for name in ['a','b']:
 root=out/name;shutil.copytree(R/'projects/animation_studio',root)
 for p in (root/'Assets/models').iterdir():p.unlink()
 log=out/(name+'.log')
 with log.open('w')as f:r=subprocess.run([R/'build/judas_model_import_cli','--recipe',root/'Imports/performer.judasimport'],cwd=R,stdout=f,stderr=subprocess.STDOUT)
 assert r.returncode==0,log
 results[name]=digest(root/'Assets/models/performer.judasmodel')
assert results['a']==results['b'],'relocated deterministic cook differs'
root=out/'a';recipe=root/'Imports/performer.judasimport';j=json.loads(recipe.read_text());j['clips'][1]['events'][0]['time']=999;recipe.write_text(json.dumps(j))
with(out/'invalid.log').open('w')as f:r=subprocess.run([R/'build/judas_model_import_cli','--recipe',recipe],cwd=R,stdout=f,stderr=subprocess.STDOUT)
assert r.returncode!=0 and digest(root/'Assets/models/performer.judasmodel')==results['a']
results['invalid_event_rejected_without_replacing_model']=True
(out/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(results)
