#include "SpriteAnimation.h"
#include "ScreenshotWriter.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "AssetDependencies.h"
#include "editor/SpriteSheetEditor.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
int main(int argc,char** argv){
    int checks=0,failures=0;
    auto check=[&](bool ok,const std::string& text){
        ++checks;
        if(!ok)++failures;
        std::cout<<(ok?"PASS ":"FAIL ")<<text<<'\n';
    };
    SpriteSheet sheet;
    sheet.texture="11111111111111111111111111111111";
    sheet.width=96;
    sheet.height=32;
    std::string error;
    check(SliceSpriteSheet(sheet,{
        32,32
    },{
        0,0
    },{
        0,0
    },error)&&sheet.frames.size()==3,"grid slicing creates stable coordinate frame keys");
    sheet.clips.push_back({
        "run",{
            {
                "cell-0-0",.25
            },{
                "cell-0-1",.5
            },{
                "cell-0-2",.25
            }
        },{
            {
                "zero","loop","",0
            },{
                "mid","strike","",.25f
            },{
                "end","end","",1
            }
        }
    });
    check(ValidSpriteSheet(sheet,error),"variable frame times and zero/end markers valid");
    SpritePlayback p;
    p.sheet="22222222222222222222222222222222";
    p.clip="run";
    SpriteSettings sprite;
    std::string key;
    check(SampleSpriteFrame(sheet,p,sprite,error,&key)&&key=="cell-0-0"&&sprite.size==glm::vec2(1),"sample starts on first frame with physical art scale");
    p.time=.25;
    check(SampleSpriteFrame(sheet,p,sprite,error,&key)&&key=="cell-0-1","exact frame boundary belongs to following frame");
    p.time=1;
    check(SampleSpriteFrame(sheet,p,sprite,error,&key)&&key=="cell-0-0","exact loop endpoint samples first frame");
    p.loop=false;
    check(SampleSpriteFrame(sheet,p,sprite,error,&key)&&key=="cell-0-2","nonloop endpoint holds last frame");
    SpriteAnimationStep step;
    p.time=0;
    p.loop=true;
    check(StepSpriteAnimation(sheet,p,.25,step)&&step.events.size()==1&&step.events[0].event.id=="mid","forward half-open interval emits marker exactly once");
    check(StepSpriteAnimation(sheet,p,0,step)&&step.events.empty(),"zero delta emits no markers");
    check(StepSpriteAnimation(sheet,p,.75,step)&&step.events.size()==2&&step.events[0].event.id=="end"&&step.events[1].event.id=="zero","loop boundary preserves distinct authored end and start events");
    check(StepSpriteAnimation(sheet,p,.01,step)&&step.events.empty(),"next step does not duplicate boundary events");
    p.time=.5;
    p.speed=-1;
    check(StepSpriteAnimation(sheet,p,.25,step)&&step.events.size()==1&&step.events[0].reverse,"reverse interval delivers reverse marker");
    p.time=0;
    p.speed=1;
    check(StepSpriteAnimation(sheet,p,2.5,step)&&step.events.size()==7,"large delta enumerates crossed markers over multiple loops");
    auto original=p;
    p.time=0;
    original=p;
    check(!StepSpriteAnimation(sheet,p,1000,step)&&step.overflow&&step.events.empty()&&EncodeSpritePlayback(p)==EncodeSpritePlayback(original),"event overflow rejects atomically, no partial publication");
    p.time=.2;
    p.playing=false;
    check(StepSpriteAnimation(sheet,p,.5,step)&&p.time==.2&&step.events.empty(),"pause freezes clock and event observations");
    p.playing=true;
    p.speed=2;
    p.loop=false;
    p.time=.9;
    check(StepSpriteAnimation(sheet,p,.2,step)&&p.time==1&&!p.playing&&step.finished,"speed and nonloop completion clamp only animation clock");
    check(StepSpriteAnimation(sheet,p,.2,step)&&!step.finished&&step.events.empty(),"completion is one boundary observation, never repeated while stopped");
    p.playing=true;
    p.speed=-1;
    p.time=.1;
    check(StepSpriteAnimation(sheet,p,.2,step)&&p.time==0&&!p.playing&&step.finished,"reverse completion reaches start");
    p.clip="absent";
    original=p;
    check(!StepSpriteAnimation(sheet,p,.1,step)&&step.error.find("unknown sprite clip")!=std::string::npos&&EncodeSpritePlayback(p)==EncodeSpritePlayback(original),"missing clip leaves time unchanged with diagnostic");
    p.clip="run";
    p.speed=std::numeric_limits<float>::quiet_NaN();
    check(!StepSpriteAnimation(sheet,p,.1,step),"NaN playback speed rejected");
    p.speed=1;
    check(!StepSpriteAnimation(sheet,p,-1,step),"negative step rejected; reverse uses signed speed");
    sheet.frames[0].name="Renamed idle";
    check(SliceSpriteSheet(sheet,{
        32,32
    },{
        0,0
    },{
        0,0
    },error)&&sheet.frames[0].name=="Renamed idle"&&sheet.clips[0].frames[0].frame=="cell-0-0","re-slice preserves stable references and renamed labels");
    auto before=EncodeSpriteSheet(sheet);
    check(!SliceSpriteSheet(sheet,{
        48,32
    },{
        0,0
    },{
        0,0
    },error)&&error.find("missing frame")!=std::string::npos&&EncodeSpriteSheet(sheet)==before,"removed referenced frame rejects re-slice atomically");
    SpriteSheet parsed;
    check(ParseSpriteSheet(before,parsed,error)&&EncodeSpriteSheet(parsed)==before,"sheet roundtrip is deterministic");
    check(!ParseSpriteSheet("{\"version\":999}",parsed,error)&&EncodeSpriteSheet(parsed)==before,"newer/malformed sheet leaves previous data intact");
    auto bad=sheet;
    bad.frames[0].pixels.z=999;
    check(!ValidSpriteSheet(bad,error),"out-of-image rectangle rejected");
    bad=sheet;
    bad.frames[0].id=bad.frames[1].id;
    check(!ValidSpriteSheet(bad,error),"duplicate stable keys rejected");
    bad=sheet;
    bad.clips[0].frames[0].seconds=0;
    check(!ValidSpriteSheet(bad,error),"zero duration rejected");
    bad=sheet;
    bad.clips[0].events[0].time=2;
    check(!ValidSpriteSheet(bad,error),"out-of-clip event rejected");
    p={
    };
    p.sheet="22222222222222222222222222222222";
    p.clip="run";
    p.time=.125;
    p.speed=-2;
    p.playing=false;
    SpritePlayback decoded;
    check(DecodeSpritePlayback(EncodeSpritePlayback(p),decoded,error)&&EncodeSpritePlayback(decoded)==EncodeSpritePlayback(p),"double phase/speed/pause serialization exact");
    Scene scene;
    auto& o=scene.CreateObject("Animated sprite");
    o.render=SceneRenderComponent{
    };
    o.render->shape=SceneShape::Sprite;
    o.render->spritePlayback=p;
    std::string text;
    SaveSceneToString(scene,text);
    Scene loaded;
    check(LoadSceneFromString(text,loaded,error)&&loaded.Objects()[0].render->spritePlayback&&EncodeSpritePlayback(*loaded.Objects()[0].render->spritePlayback)==EncodeSpritePlayback(p),"scene/prefab descriptor carries named clip and exact phase");
    std::set<AssetId> ids;
    CollectSceneAssetReferences(scene,ids);
    check(ids.count(p.sheet)==1,"scene dependency closure includes sprite sheet");
    std::string digestA,digestB;
    check(ComputeSceneFingerprint(scene,digestA,error),"animated scene fingerprint valid");
    scene.Objects()[0].render->spritePlayback->speed=3;
    check(ComputeSceneFingerprint(scene,digestB,error)&&digestA!=digestB,"playback semantics affect scene fingerprint");
    if(argc>1){
        auto root=std::filesystem::path(argv[1]);
        std::filesystem::create_directories(root);
        std::filesystem::create_directories(root/"Assets");
        check(WriteRgbPng((root/"Assets/image.png").string(),2,2,std::vector<unsigned char>(12,255)),"author image for actual editor sheet-creation workflow");
        AssetDatabase db;db.Scan(root.string(),(root/"Assets").string());AssetRecord image,created,reused;
        check(db.Track((root/"Assets/image.png").string(),image,error)&&CreateSpriteSheetAsset(db,image.id,created,error),"image context action creates and tracks a real typed sprite sheet");
        check(CreateSpriteSheetAsset(db,image.id,reused,error)&&created.id==reused.id,"repeating image action reuses existing stable sheet identity");
        auto path=root/"sheet.judassprite";
        check(SaveSpriteSheet(path.string(),sheet,error),"validated asset writes");
        SpriteSheetEditor editor;
        AssetRecord record;
        record.id=p.sheet;
        record.path=path.string();
        record.type=AssetType::SpriteSheet;
        check(editor.Open(record,error)&&!editor.Dirty(),"editor opens clean reusable draft");
        editor.Remember();
        editor.draft.frames[0].name="Friendly name";
        check(editor.Dirty(),"editor draft detects edits");
        check(!editor.Open(record,error),"unsaved editor draft cannot silently disappear on switch");
        check(editor.Save(error)&&!editor.Dirty(),"editor explicit save clears dirty state");
        auto external=sheet;
        external.frames[1].name="External edit";
        check(SaveSpriteSheet(path.string(),external,error),"independent reimport changes asset");
        editor.draft.frames[0].name="pending";
        check(!editor.Save(error)&&error.find("changed on disk")!=std::string::npos,"concurrent source changes cannot be overwritten by stale draft");
        std::set<std::string> auxiliary;
        ids.clear();
        check(CollectAssetDependencies(record,ids,auxiliary,error)&&ids.count(sheet.texture),"export dependency closure includes underlying texture");
    }
    auto decimal=sheet;decimal.clips={{"decimal",{{"cell-0-0",.1},{"cell-0-1",.2}},{{"endpoint","hit","",.3}}}};
    SpritePlayback decimalPlayback;decimalPlayback.sheet=p.sheet;decimalPlayback.clip="decimal";decimalPlayback.loop=false;
    check(StepSpriteAnimation(decimal,decimalPlayback,.3,step)&&step.events.size()==1,"decimal endpoint marker is not rounded beyond its nonloop clip boundary");
    auto emptyClip=sheet;emptyClip.clips[0].frames.clear();p.clip="run";
    check(!SampleSpriteFrame(emptyClip,p,sprite,error)&&error.find("frame count")!=std::string::npos,"unvalidated empty native clip rejects without dereferencing an absent frame");
    std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";
    return failures?1:0;
}
