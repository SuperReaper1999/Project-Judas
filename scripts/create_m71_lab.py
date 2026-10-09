#!/usr/bin/env python3
"""Create the small original M71 lab through Judas named authoring.

All geometry is ordinary box/sphere render and collider components. Scripts
use public entity APIs. Existing edited projects need explicit --overwrite.
"""
from __future__ import annotations
import argparse, copy, hashlib, json, re, shutil, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
LICENSE="Original M71 lab scripts and layout: CC0-1.0. https://creativecommons.org/publicdomain/zero/1.0/\nFont licensing is retained separately.\n"

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--tool",default="build/judas_scene_author");parser.add_argument("--project",default="projects/kinematic_lab");parser.add_argument("--overwrite",action="store_true")
    args=parser.parse_args();project=ROOT/args.project;tool=ROOT/args.tool;config=project/"kinematic_lab.judasproj"
    if config.exists() and not args.overwrite:raise SystemExit("Refusing to overwrite an edited lab; use a fresh --project or explicit --overwrite")
    for folder in ["Assets/scripts","Assets/fonts","Assets/ui","Assets/prefabs","Scenes","Authoring"]:(project/folder).mkdir(parents=True,exist_ok=True)
    def run(*arguments):
        result=subprocess.run([str(tool),*[str(x) for x in arguments]],cwd=ROOT,text=True,capture_output=True)
        if result.returncode:raise RuntimeError(" ".join(map(str,arguments))+"\n"+result.stdout+result.stderr)
        return result.stdout.strip()
    def patch(kind,path,data):
        edits=project/"Authoring/edit.json";edits.write_text(json.dumps([{"op":"replace","path":"/data","value":data}],indent=2)+"\n")
        run("--patch",kind,path,path,edits,"--overwrite")
    def template(components,name):
        path=project/"Authoring"/("template-"+components.replace(",","-")+".json")
        run("--object-template",components,name,path,"--overwrite");return copy.deepcopy(json.loads(path.read_text())["data"]["objects"][0])
    def track(path):
        metadata=Path(str(path)+".judasmeta")
        if metadata.exists():return re.search(r'id "([^"]+)"',metadata.read_text()).group(1)
        identity=hashlib.md5(("M71 original kinematic lab:"+str(path.relative_to(project))).encode()).hexdigest()
        run("--track-asset",config,path,identity);return identity
    run("--create","project",config,"--overwrite");settings=json.loads(config.read_text())["data"]
    settings.update(name="Kinematic Lab",legacyGameplay=False,startupScene="Scenes/main.judas",exportScenes=["Scenes/main.judas"],saveIdentity=hashlib.sha256(b"Judas original M71 kinematic lab").hexdigest())
    for name,key in [("stop","T"),("reverse","V"),("spin","O"),("fast","J"),("dynamic","G"),("kinematic","K"),("remove","X"),("rider_one","F1"),("rider_two","F2"),("rider_rotated","F3"),("save","F6"),("load","F7"),("reset","R")]:
        settings["input"]=[x for x in settings["input"] if x["name"]!=name]
        settings["input"].append({"name":name,"axis":False,"bindings":[{"control":"key:"+key,"scale":1,"deadzone":0}]})
    patch("project",config,settings)
    scripts={}
    for name in ["lab.js","contact.js"]:
        source=ROOT/"projects/kinematic_lab/Assets/scripts"/name;destination=project/"Assets/scripts"/name
        if source.resolve()!=destination.resolve():shutil.copy2(source,destination)
        scripts[name]=track(destination)
    font=project/"Assets/fonts/DejaVuSans.ttf";shutil.copy2(ROOT/"assets/fonts/DejaVuSans.ttf",font);shutil.copy2(ROOT/"assets/fonts/DejaVuSans-LICENSE.txt",font.parent/"LICENSE.txt");font_id=track(font)
    ui=project/"Assets/ui/lab.judasui";run("--create","ui",ui,"--overwrite");data=json.loads(ui.read_text())["data"];base=data["elements"][0];base.update(size=[0,0],anchorMax=[1,1]);data["elements"]=[base]
    def element(identity,kind,text,offset,size,parent="canvas",**options):
        e=copy.deepcopy(base);e.update(id=identity,parent=parent,kind=kind,anchorMax=[0,0],size=size,offset=offset,text=text,font=font_id,fontSize=18,background=[0,0,0,0]);e.update(options);data["elements"].append(e)
    element("title",2,"KINEMATIC LAB / M71",[20,15],[1100,32],fontSize=24)
    element("help",2,"F1 lift rider | F2 rotating rider | F3 rotated-gravity rider | WASD walk | Space jump\nT stop/resume | V reverse | O pusher spin | J fast crossing (R first) | G/K lift dynamic/kinematic\nX remove pusher | F6/F7 save/load | R reload | Esc pause",[20,50],[1220,85],wrap=True)
    element("status",2,"Loading ordinary kinematic colliders…",[20,142],[1200,135],wrap=True)
    element("pause",1,"",[0,0],[360,160],anchorMin=[.5,.5],anchorMax=[.5,.5],align=[.5,.5],visible=False,padding=[15,15,15,15],flow=2,background=[.05,.09,.13,.95])
    element("resume",4,"RESUME",[0,0],[330,55],parent="pause",background=[.15,.3,.42,1]);element("reload",4,"RELOAD LAB",[0,0],[330,55],parent="pause",background=[.15,.3,.42,1])
    patch("ui",ui,data);ui_id=track(ui)
    path=project/"Scenes/main.judas";run("--create","scene",path,"--overwrite");scene=json.loads(path.read_text())["data"];scene["settings"]["name"]="M71 / ordinary kinematic lab";scene["settings"]["next-id"]=1000
    def obj(identity,components,name,position,fields=None):
        o=template(components,name);o["id"]=identity;o["fields"]["position"]=position
        if fields:o["fields"].update(fields)
        scene["objects"].append(o);return o
    def box(identity,name,position,half,motion="static",color=(.4,.5,.6)):
        return obj(identity,"render,body",name,position,{"render.half-extents":half,"body.half-extents":half,"body":{"motion":{"symbol":motion},"shape":{"symbol":"box"}},"body.mass":20 if motion=="kinematic" else 2,"body.friction":.9,"body.restitution":0,"render.color":list(color)})
    box(1,"Ordinary floor",[2,-.25,0],[12,.25,9],color=(.12,.17,.2))
    obj(2,"gravity","Uniform gravity for ordinary lanes",[0,0,0],{"gravity":{"kind":{"symbol":"uniform"},"magnitude":9.81},"gravity.region":{"shape":{"symbol":"box"},"halfExtents":[10,20,20]}})
    box(10,"Script-commanded pusher",[-6,.35,2],[.35,.35,.55],"kinematic",(.9,.5,.15))
    box(11,"Genuinely dynamic pusher crate",[-3,.3,2],[.3,.3,.3],"dynamic",(.25,.65,.85))
    box(12,"Static control pusher",[-6,.35,5],[.35,.35,.55],color=(.4,.4,.4))
    box(13,"Static control dynamic crate",[-3,.3,5],[.3,.3,.3],"dynamic",(.25,.65,.85))
    box(20,"Translating lifting support",[-1,.75,-5],[2,.25,2],"kinematic",(.25,.65,.4))
    box(21,"Dynamic lift rider crate",[-1.7,1.3,-4.5],[.3,.3,.3],"dynamic",(.85,.3,.25))
    box(30,"Continuously rotating support",[6,.75,-5],[2,.25,2],"kinematic",(.5,.35,.7))
    box(31,"Dynamic rotating rider crate",[5.3,1.3,-4.5],[.3,.3,.3],"dynamic",(.85,.3,.25))
    rq=[.7071067811865476,0,0,-.7071067811865476]
    support=box(50,"Support in rotated gravity",[13,1.5,-4],[2,.25,2],"kinematic",(.2,.65,.7));support["fields"]["rotation"]=rq
    crate=box(51,"Dynamic crate in rotated gravity",[13.55,2.2,-3.5],[.3,.3,.3],"dynamic",(.85,.3,.25));crate["fields"]["rotation"]=rq
    obj(52,"gravity","Rotated gravity context",[14,2,0],{"rotation":rq,"gravity":{"kind":{"symbol":"uniform"},"magnitude":9.81},"gravity.region":{"shape":{"symbol":"box"},"halfExtents":[10,3,10]}})
    for identity,position,q in [(40,[-.4,1.92,-5.4],[1,0,0,0]),(41,[6.6,1.92,-5.4],[1,0,0,0]),(42,[14.17,.9,-4.4],rq)]:
        obj(identity,"render,motor","Independent motor rider",position,{"rotation":q,"render":{"symbol":"sphere"},"render.radius":.3,"render.color":[.95,.85,.2],"motor.radius":.25,"motor.halfHeight":.65,"motor.offset":[0,0,0]})
    controller=obj(90,"empty","Public JS lab controls",[0,0,0],{"scripts":1,"script.0.id":1,"script.0.asset":scripts["lab.js"],"script.0.enabled":True,"script.0.properties":{"json":{}},"ui.asset":ui_id,"ui.name":"kinematic_lab","ui.enabled":True})
    for o in scene["objects"]:
        if o["id"] in [10,20,30,50]:o["fields"].update({"scripts":1,"script.0.id":1,"script.0.asset":scripts["contact.js"],"script.0.enabled":True,"script.0.properties":{"json":{}}})
    patch("scene",path,scene)
    prefab=project/"Assets/prefabs/platform.judasprefab";run("--create","prefab",prefab,"--overwrite");prefab_data=json.loads(prefab.read_text())["data"]
    source=copy.deepcopy(next(o for o in scene["objects"] if o["id"]==20));source["id"]=1;source["fields"]["position"]=[0,0,0];prefab_data["objects"]=[source];prefab_data["settings"]["next-id"]=2
    patch("prefab",prefab,prefab_data);track(prefab)
    (project/"Assets/LICENSE.txt").write_text(LICENSE)
    print("Created normal public-JS kinematic lab:",config)

if __name__=="__main__":main()
