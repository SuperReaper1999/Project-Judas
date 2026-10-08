#!/usr/bin/env python3
"""Create the original CC0 character lab through shipped named authoring/import.

The geometry generator below emits commodity glTF source art. Scene, prefab,
project, UI and ragdoll mapping writes go through judas_scene_author; this is
not a private engine-format emitter or a second skeleton/import pipeline.
Existing edited projects are never overwritten unless --overwrite is explicit.
"""
from __future__ import annotations
import argparse, base64, copy, hashlib, json, math, re, shutil, struct, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LICENSE = "Original M70 source geometry, animation, project scripts and layout are dedicated to the public domain under CC0-1.0. https://creativecommons.org/publicdomain/zero/1.0/\nNo restricted Skate/M66 artwork or cooked derivative is included. Font licensing is retained separately.\n"


def make_source(path: Path, variant=False):
    """65 original source joints, helper branches, six mesh parts and two skins."""
    blob = bytearray(); views = []; accessors = []; nodes = []; semantic = {}
    parents = []; positions = []
    width, limb = ((1.18, .92) if variant else (1., 1.))
    def bone(key, parent, position):
        i = len(nodes); semantic[key] = i
        name = "Q%02d" % i if variant else key
        nodes.append({"name": name, "translation": position}); parents.append(parent)
        positions.append([position[k] + (positions[parent][k] if parent >= 0 else 0) for k in range(3)])
        if parent >= 0: nodes[parent].setdefault("children", []).append(i)
        return i
    origin = bone("OriginHelper", -1, [0, 0, 0])
    pivot = bone("Pivot", origin, [0, .98 * limb, 0])
    spine0 = bone("Vertebra0", pivot, [0, .18, 0])
    spine1 = bone("Vertebra1", spine0, [0, .24, 0])
    neck = bone("NeckConnector", spine1, [0, .23, 0])
    head = bone("Cranial", neck, [0, .14, 0])
    for side, sign in [("A", -1), ("B", 1)]:
        collar = bone("Collar" + side, spine1, [sign * .15 * width, .09, 0])
        upper = bone("Shoulder" + side, collar, [sign * .16 * width, 0, 0])
        middle = bone("Elbow" + side, upper, [sign * .28 * width, -.3 * limb, -.04])
        hand = bone("Palm" + side, middle, [sign * .25 * width, -.26 * limb, .04])
        for finger in range(5):
            previous = hand
            for segment in range(3):
                previous = bone("Digit%s%d%d" % (side, finger, segment), previous,
                                [sign * (.028 if segment else .036), 0, (finger - 2) * .018 if segment == 0 else 0])
        thigh = bone("Thigh" + side, pivot, [sign * .15 * width, -.04, 0])
        shin = bone("Shin" + side, thigh, [0, -.43 * limb, .055])
        foot = bone("Sole" + side, shin, [0, -.44 * limb, -.055])
        bone("ToeHelper" + side, foot, [0, -.025, -.13])
    # Deliberately unmapped helper branches must remain in the imported rig.
    while len(nodes) < 65:
        n = len(nodes)
        bone("Auxiliary%02d" % n, [pivot, spine0, spine1, neck, head][n % 5], [.018 * (n % 3 - 1), .025, .015])
    def accessor(data, kind, component=5126):
        while len(blob) % 4: blob.append(0)
        start = len(blob); flat = [x for row in data for x in (row if isinstance(row, (list, tuple)) else [row])]
        blob.extend(struct.pack("<" + {5126: "f", 5123: "H"}[component] * len(flat), *flat))
        views.append({"buffer": 0, "byteOffset": start, "byteLength": len(blob) - start})
        accessors.append({"bufferView": len(views)-1, "componentType": component, "count": len(data), "type": kind})
        return len(accessors)-1
    groups = [["Pivot", "Vertebra0", "Vertebra1"], ["NeckConnector", "Cranial"],
              ["ShoulderA", "ElbowA", "PalmA"], ["ShoulderB", "ElbowB", "PalmB"],
              ["ThighA", "ShinA", "SoleA"], ["ThighB", "ShinB", "SoleB"]]
    skin_joints = [list(range(65)), list(reversed(range(65)))]
    skins = []
    for order in skin_joints:
        binds = []
        for n in order:
            p = positions[n]; binds.append([1,0,0,0,0,1,0,0,0,0,1,0,-p[0],-p[1],-p[2],1])
        skins.append({"joints": order, "skeleton": origin, "inverseBindMatrices": accessor(binds, "MAT4")})
    colors = [[.19,.46,.69,1],[.85,.65,.35,1],[.76,.28,.21,1],[.76,.28,.21,1],[.24,.64,.39,1],[.24,.64,.39,1]]
    materials = [{"name": "Original part %d" % i, "pbrMetallicRoughness": {"baseColorFactor": c, "metallicFactor": 0, "roughnessFactor": .65}} for i,c in enumerate(colors)]
    meshes = []
    # Weighted box sections are intentionally readable placeholder character art.
    # Both skins use the complete rig; the second reorders its joint palette.
    for part, keys in enumerate(groups):
        p = []; normals = []; uv = []; joint_ids = []; weights = []; indices = []
        order = skin_joints[part % 2]
        for key in keys:
            n = semantic[key]; center = positions[n].copy()
            if key == "Pivot": center[1] += .035; half = [.19 * width,.13,.115]
            elif key.startswith("Vertebra"): center[1] += .1; half = [.22 * width,.13,.12]
            elif key == "Cranial": center[1] += .07; half = [.145,.16,.13]
            elif key == "NeckConnector": half = [.065,.12,.065]
            elif key.startswith("Palm"): half = [.1 * width,.07,.085]
            elif key.startswith("Sole"): center[1] -= .025; center[2] -= .05; half = [.11,.055,.17]
            else:
                children = nodes[n].get("children", []); end = positions[children[0]] if children else positions[n]
                center = [(positions[n][k] + end[k]) / 2 for k in range(3)]
                half = [max(.075, abs(end[k]-positions[n][k])/2 + .045) for k in range(3)]
            for normal, axes in [([1,0,0],(0,1,2)),([-1,0,0],(0,2,1)),([0,1,0],(1,2,0)),([0,-1,0],(1,0,2)),([0,0,1],(2,0,1)),([0,0,-1],(2,1,0))]:
                start = len(p)
                for a,b in [(-1,-1),(1,-1),(1,1),(-1,1)]:
                    v = center.copy(); v[axes[0]] += normal[axes[0]] * half[axes[0]]
                    v[axes[1]] += a * half[axes[1]]; v[axes[2]] += b * half[axes[2]]
                    p.append(v); normals.append(normal); uv.append([(a+1)/2,(b+1)/2])
                    joint_ids.append([order.index(n),0,0,0]); weights.append([1,0,0,0])
                indices.extend([start,start+1,start+2,start,start+2,start+3])
        attrs = {"POSITION": accessor(p,"VEC3"), "NORMAL": accessor(normals,"VEC3"), "TEXCOORD_0": accessor(uv,"VEC2"), "JOINTS_0": accessor(joint_ids,"VEC4",5123), "WEIGHTS_0": accessor(weights,"VEC4")}
        meshes.append({"name": "Part%d" % part, "primitives": [{"attributes": attrs, "indices": accessor(indices,"SCALAR",5123), "material": part}]})
        nodes.append({"name": "VisualPart%d" % part,"mesh": part,"skin": part % 2})
    times = accessor([0,.5,1,1.5,2], "SCALAR")
    def rotation(axis, angle): return [axis[0]*math.sin(angle/2),axis[1]*math.sin(angle/2),axis[2]*math.sin(angle/2),math.cos(angle/2)]
    def clip(name, tracks):
        samplers = []; channels = []
        for key, path_name, values in tracks:
            samplers.append({"input": times,"output": accessor(values,"VEC3" if path_name in ("translation","scale") else "VEC4"),"interpolation":"LINEAR"})
            channels.append({"sampler": len(samplers)-1,"target":{"node":semantic[key],"path":path_name}})
        return {"name":name,"samplers":samplers,"channels":channels}
    clips = [clip("Idle", [("Vertebra0","rotation",[rotation([0,0,1],a) for a in [0,.018,0,-.018,0]])]),
             clip("Carry", [("ShoulderA","rotation",[rotation([0,0,1],a) for a in [-.65,-.7,-.65,-.6,-.65]]),
                            ("ShoulderB","rotation",[rotation([0,0,1],a) for a in [.65,.7,.65,.6,.65]])]),
             clip("Wave", [("ElbowA","rotation",[rotation([0,0,1],a) for a in [0,-.3,-.6,-.3,0]])]),
             clip("HeadHidden", [("Cranial","scale",[[0,0,0]]*5)])]
    doc = {"asset":{"version":"2.0","generator":"Judas original CC0 M70 full multipart rig"},"scene":0,"scenes":[{"nodes":[origin]+list(range(65,len(nodes)))}],"nodes":nodes,"skins":skins,"meshes":meshes,"materials":materials,"animations":clips,"buffers":[{"byteLength":len(blob),"uri":"data:application/octet-stream;base64,"+base64.b64encode(blob).decode()}],"bufferViews":views,"accessors":accessors}
    path.parent.mkdir(parents=True,exist_ok=True); path.write_text(json.dumps(doc,separators=(",",":"))+"\n")
    return {key:nodes[index]["name"] for key,index in semantic.items()}


def main():
    parser=argparse.ArgumentParser(); parser.add_argument("--tool", default="build/judas_scene_author"); parser.add_argument("--project", default="projects/character_lab"); parser.add_argument("--sources-only",action="store_true"); parser.add_argument("--overwrite",action="store_true")
    args=parser.parse_args(); project=ROOT/args.project; config=project/"character_lab.judasproj"; tool=ROOT/args.tool
    if config.exists() and not args.overwrite: raise SystemExit("Refusing to overwrite an edited character lab; use a fresh --project or explicit --overwrite")
    for folder in ["Scenes","Authoring","Sources","Assets/models","Assets/scripts","Assets/fonts","Assets/ui","Assets/prefabs"]: (project/folder).mkdir(parents=True,exist_ok=True)
    mappings = {"figure":make_source(project/"Sources/figure.gltf"),"alternate":make_source(project/"Sources/alternate.gltf",True)}
    (project/"Sources/LICENSE.txt").write_text(LICENSE); (project/"Assets/LICENSE.txt").write_text(LICENSE)
    (project/"Authoring/joint-names.json").write_text(json.dumps(mappings,indent=2)+"\n")
    if args.sources_only: print("CC0 full multipart source fixtures:", project); return
    def run(*cmd):
        result=subprocess.run([str(tool),*[str(x) for x in cmd]],cwd=ROOT,text=True,capture_output=True)
        if result.returncode: raise RuntimeError(" ".join(map(str,cmd))+"\n"+result.stdout+result.stderr)
        return result.stdout.strip()
    def write(path,data): path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n")
    def patch(kind,path,data):
        edits=project/"Authoring/edit.json";write(edits,[{"op":"replace","path":"/data","value":data}]);run("--patch",kind,path,path,edits,"--overwrite")
    def template(components,name):
        path=project/"Authoring"/("template-"+components.replace(",","-")+".json");run("--object-template",components,name,path,"--overwrite");return json.loads(path.read_text())["data"]["objects"][0]
    def track(path):
        metadata=Path(str(path)+".judasmeta")
        if metadata.exists():return re.search(r'id "([^"]+)"',metadata.read_text()).group(1)
        identity=hashlib.md5(("M70 Character Lab:"+str(path.relative_to(project))).encode()).hexdigest();run("--track-asset",config,path,identity);return identity
    def ik_settings(n):
        chains=[["palmA","ShoulderA","ElbowA","PalmA"],["palmB","ShoulderB","ElbowB","PalmB"],["soleA","ThighA","ShinA","SoleA"],["soleB","ThighB","ShinB","SoleB"]]
        joints=["Pivot","Vertebra0","Vertebra1","ShoulderA","ElbowA","PalmA","ShoulderB","ElbowB","PalmB","ThighA","ShinA","SoleA","ThighB","ShinB","SoleB"]
        return dict(enabled=True,bodyRoot=n["Pivot"],rootRotation=True,rootMin=[-.18,-.22,-.18],rootMax=[.18,.22,.18],spine=[n["Vertebra0"],n["Vertebra1"]],iterations=64,damping=.02,positionTolerance=.005,orientationTolerance=.017453293,chains=[dict(id=c[0],joints=[n[k] for k in c[1:]]) for c in chains],limits=[dict(joint=n[k],frame=[0,0,0,1],min=[-2.6]*3,max=[2.6]*3,preferred=[0,0,0],preferenceWeight=0) for k in joints],targets=[])
    def physical_settings(n):
        return dict(enabled=True,regions=[dict(id="upper",joints=[n[k] for k in ["Vertebra0","Vertebra1","ShoulderA","ElbowA","PalmA","ShoulderB","ElbowB","PalmB"]],enabled=True,stiffness=32,damping=5,maxTorque=24,effortWeight=1,poseWeight=1)])
    run("--create","project",config,"--overwrite");p=json.loads(config.read_text())["data"]
    p.update(name="Character Lab",legacyGameplay=False,startupScene="Scenes/wall.judas",exportScenes=["Scenes/wall.judas","Scenes/board.judas","Scenes/physical.judas"],saveIdentity=hashlib.sha256(b"Judas M70 original character lab").hexdigest())
    p["classification"]["collision"]={"nextId":3,"entries":[{"id":0,"name":"Default"},{"id":1,"name":"Motor"},{"id":2,"name":"Articulation"}]}
    for name,key in [("wall","F1"),("board","F2"),("physical","F3"),("adjust","T"),("impossible","I"),("clips","C"),("layer","L"),("head","H"),("impact","J"),("drive","K"),("active","G"),("passive","P"),("animated","N"),("spawn","Z"),("save","F6"),("load","F7"),("reset","R")]:
        p["input"]=[x for x in p["input"] if x["name"]!=name];p["input"].append({"name":name,"axis":False,"bindings":[{"control":"key:"+key,"scale":1,"deadzone":0}]})
    patch("project",config,p)
    font=project/"Assets/fonts/DejaVuSans.ttf";shutil.copy2(ROOT/"assets/fonts/DejaVuSans.ttf",font);shutil.copy2(ROOT/"assets/fonts/DejaVuSans-LICENSE.txt",font.parent/"LICENSE.txt");font_id=track(font)
    model_ids={}
    for name in mappings:
        destination="Assets/models/"+name+".judasmodel"
        existing=[p for p in (project/"Imports").glob("*.judasimport") if json.loads(p.read_text()).get("output")==destination]
        recipe=str(existing[0]) if existing else run("--create-import",project,project/"Sources"/(name+".gltf"),destination).splitlines()[-1];run("--import-model",recipe)
        model_ids[name]=track(project/"Assets/models"/(name+".judasmodel"))
    scripts={}
    for name in ["lab.js","math.js","mapping.js"]:
        source=ROOT/"projects/character_lab/Assets/scripts"/name;dest=project/"Assets/scripts"/name
        if source.resolve()!=dest.resolve():shutil.copy2(source,dest)
        scripts[name]=track(dest)
    ui=project/"Assets/ui/lab.judasui";run("--create","ui",ui,"--overwrite");data=json.loads(ui.read_text())["data"];base=data["elements"][0];base.update(size=[0,0],anchorMax=[1,1]);data["elements"]=[base]
    def element(id,kind,text,offset,size,parent="canvas",**kw):
        e=copy.deepcopy(base);e.update(id=id,parent=parent,kind=kind,anchorMax=[0,0],size=size,offset=offset,text=text,font=font_id,fontSize=18,background=[0,0,0,0]);e.update(kw);data["elements"].append(e)
    element("title",2,"CHARACTER LAB / M70",[20,15],[1000,32],fontSize=24)
    element("help",2,"F1 wall | F2 board | F3 physical | T targets | I impossible | C crossfade | L carry | H head\nWASD motor | Mouse inspect | J impact | K drive | G active | P passive | N animation | Z spawn (physical) | F6/F7 save/load | R reload | Esc pause",[20,50],[1220,78],wrap=True)
    element("status",2,"Loading full multipart model…",[20,130],[1200,135],wrap=True)
    element("pause",1,"",[0,0],[360,220],anchorMin=[.5,.5],anchorMax=[.5,.5],align=[.5,.5],visible=False,padding=[15,15,15,15],flow=2,background=[.05,.09,.13,.95])
    element("resume",4,"RESUME",[0,0],[330,55],parent="pause",background=[.15,.3,.42,1]);element("reload",4,"RELOAD AUTHORED LAB",[0,0],[330,55],parent="pause",background=[.15,.3,.42,1]);patch("ui",ui,data);ui_id=track(ui)
    def scene(label):
        path=project/"Scenes"/(label+".judas");run("--create","scene",path,"--overwrite");data=json.loads(path.read_text())["data"];data["settings"]["name"]="M70 / "+label;data["settings"]["next-id"]=1000
        def obj(id,components,name,pos,fields=None):
            o=template(components,name);o["id"]=id;o["fields"]["position"]=pos
            if fields:o["fields"].update(fields)
            data["objects"].append(o);return o
        obj(1,"render,body","Ordinary floor",[0,-.25,0],{"render.half-extents":[12,.25,12],"body.half-extents":[12,.25,12],"render.color":[.12,.17,.2]})
        obj(2,"gravity","Ordinary uniform gravity",[0,0,0],{"gravity":{"kind":{"symbol":"uniform"},"magnitude":9.81},"gravity.region":{"shape":{"symbol":"box"},"halfExtents":[100,100,100]}})
        for id,x,model in [(10,-1.8,"figure"),(11,1.8,"alternate")]:
            obj(id,"render,animation,motor","Original multipart instance" if model=="figure" else "Renamed/proportioned independent instance",[x,0,0],{"render":{"symbol":"mesh"},"render.mesh-asset":model_ids[model],"animation.clip":"Idle","motor.offset":[0,.9,0],"motor.radius":.23,"motor.halfHeight":.65,"motor.collisionLayer":1,"motor.collisionMask":1,"render.color":[1,1,1]})
        obj(90,"empty","Project JS controls",[0,0,0],{"scripts":1,"script.0.id":1,"script.0.asset":scripts["lab.js"],"script.0.enabled":True,"script.0.properties":{"json":{"mode":label,"namesJson":json.dumps(mappings,separators=(",",":")),"actorA":{"entity":"10"},"actorB":{"entity":"11"}}},"ui.asset":ui_id,"ui.name":"character_lab","ui.enabled":True})
        for id,actor,joint in [(30,10,mappings["figure"]["PalmB"]),(31,11,mappings["alternate"]["PalmB"])]:
            obj(id,"render","Resolved palm socket prop",[0,0,0],{"render.half-extents":[.06,.06,.17],"render.color":[.95,.65,.1],"socket.target":actor,"socket.joint":joint,"socket.enabled":True,"socket.position":[0,0,-.13],"socket.rotation":[1,0,0,0],"socket.scale":[1,1,1]})
        if label=="wall":
            for id,x in [(20,-1.8),(21,1.8)]:
                obj(id,"render,body","Ordinary wall contact reference",[x,.95,-.42],{"render.half-extents":[.9,.95,.08],"body.half-extents":[.9,.95,.08],"render.color":[.26,.29,.32]})
                obj(id+2,"render,body","Ordinary ledge contact reference",[x,1.60,-.39],{"render.half-extents":[.9,.04,.17],"body.half-extents":[.9,.04,.17],"render.color":[.45,.5,.54]})
        elif label=="board":
            for id,x in [(20,-1.8),(21,1.8)]:
                obj(id,"body","Actual fixed-step support collider",[x,.015,0],{"body.half-extents":[.65,.035,.55]})
                obj(id+2,"render","Interpolated support presentation",[x,.015,0],{"render.half-extents":[.65,.035,.55],"render.color":[.8,.4,.13]})
        elif label=="physical":
            obj(20,"render,body","Ordinary impact obstacle",[0,.7,-2],{"render.half-extents":[1,.7,.35],"body.half-extents":[1,.7,.35],"render.color":[.3,.5,.6]})
            obj(21,"render,body","Ordinary dynamic prop",[0,2,1],{"body":{"motion":{"symbol":"dynamic"},"shape":{"symbol":"box"}},"body.mass":3,"render.half-extents":[.3,.3,.3],"body.half-extents":[.3,.3,.3],"render.color":[.85,.3,.25]})
        patch("scene",path,data)
        for id,model in [(10,"figure"),(11,"alternate")]:
            # A physical subset is explicit. The independently hidden Cranial
            # visual branch follows NeckConnector; no zero-scale collider exists.
            keys=[mappings[model][k] for k in ["Pivot","Vertebra0","Vertebra1","NeckConnector","ShoulderA","ElbowA","PalmA","ShoulderB","ElbowB","PalmB","ThighA","ShinA","SoleA","ThighB","ShinB","SoleB"]]
            run("--fit-skeleton",path,config,id,",".join(keys),.085,path,"--overwrite")
        fitted=json.loads(path.read_text())["data"]
        for o in fitted["objects"]:
            if o["id"] not in [10,11]:continue
            fields=o["fields"];fields["ragdoll.self-collision"]=True
            names=mappings["alternate" if o["id"]==11 else "figure"]
            fields["ragdoll.physical-animation"]=json.dumps(physical_settings(names),separators=(",",":"))
            if label!="physical":fields["animation.full-body"]=json.dumps(ik_settings(names),separators=(",",":"))
            for index in range(fields["ragdoll.bones"]):
                prefix="ragdoll.bone."+str(index)+"."
                fields[prefix+"layer"]=2;fields[prefix+"mask"]=5
                fields[prefix+"mass"]=4 if index<3 else 1
                fields[prefix+"limits"]=True;fields[prefix+"lower"]=-1.6;fields[prefix+"upper"]=1.6
                fields[prefix+"rotational-resistance"]=.12
        patch("scene",path,fitted)
        return path
    paths=[scene(label) for label in ["wall","board","physical"]]
    # A normal authored source object, including its accepted skeleton mapping.
    doc=json.loads(paths[2].read_text());source=copy.deepcopy(next(o for o in doc["data"]["objects"] if o["id"]==10));source["id"]=1;source["fields"]["position"]=[0,0,0]
    prefab=project/"Assets/prefabs/figure.judasprefab";run("--create","prefab",prefab,"--overwrite");pdoc=json.loads(prefab.read_text())["data"];pdoc["objects"]=[source];pdoc["settings"]["next-id"]=2;patch("prefab",prefab,pdoc);prefab_id=track(prefab)
    for path in paths:
        data=json.loads(path.read_text())["data"];control=next(o for o in data["objects"] if o["id"]==90)
        values=control["fields"]["script.0.properties"]
        values=json.loads(values) if isinstance(values,str) else values["json"]
        values["prefab"]=prefab_id;control["fields"]["script.0.properties"]={"json":values};patch("scene",path,data)
    # The last-edit scratch is not an asset nor an authoritative authoring record.
    (project/"Authoring/edit.json").unlink(missing_ok=True)
    print("Ordinary public-authored character lab:", config)


if __name__ == "__main__": main()
