#!/usr/bin/env python3
"""Larger Workshop through the shipped recipe author, then identical baseline/final
performance binaries. Fresh owned outputs; source project is never written."""
import argparse,json,os,shutil,subprocess,time
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--baseline-bin',type=Path,required=True);p.add_argument('--final-bin',type=Path,required=True);a=p.parse_args();out=a.output.resolve()
 if out.exists():p.error('Output exists; choose a fresh directory.')
 work=out/'owned';project=work/'projects/world_workshop';shutil.copytree(R/'projects/world_workshop',project,ignore=shutil.ignore_patterns('.cache','__pycache__','Saves'));(work/'assets').symlink_to(R/'assets',target_is_directory=True)
 recipe=project/'Authoring/gallery-0.judasrecipe';j=json.loads(recipe.read_text());j['data']['count']=2000;recipe.write_text(json.dumps(j,indent=2)+'\n')
 generated=project/'Scenes/expanded.judas';command=[str(R/'build/judas_scene_author'),'--generate',str(recipe),str(project/'Scenes/workshop.judas'),str(project/'world_workshop.judasproj'),str(generated)]
 g=subprocess.run(command,cwd=R,capture_output=True,text=True);(out/'generation.log').write_text(g.stdout+g.stderr)
 if g.returncode:raise RuntimeError('ordinary recipe generation failed')
 shutil.copyfile(generated,project/'Scenes/workshop.judas');rows=[]
 for label,binary in [('baseline',a.baseline_bin.resolve()),('final',a.final_bin.resolve())]:
  for repeat in range(2):
   destination=out/f'{label}-{repeat}';t=time.monotonic();r=subprocess.run(['/usr/bin/time','-v','-o',str(out/f'{label}-{repeat}-time.txt'),str(binary),str(destination)],cwd=work,capture_output=True,text=True,timeout=300)
   (out/f'{label}-{repeat}.log').write_text(r.stdout+r.stderr);rows.append({'label':label,'repeat':repeat,'exit':r.returncode,'wallMs':(time.monotonic()-t)*1000,'command':[str(binary),str(destination)],'cwd':str(work)});(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
   if r.returncode:raise RuntimeError(label+' failed')
   print(label,repeat,'complete',flush=True)
if __name__=='__main__':main()
