#include "SpriteAnimation.h"
#include "PlatformServices.h"
#include "AssetDatabase.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <set>
#include <sstream>
using Json=nlohmann::json;
namespace {
    bool Key(const std::string& s){
        return !s.empty()&&s.size()<=128&&s.find_first_of("\r\n\t") == std::string::npos;
    }
    const SpriteFrame* Frame(const SpriteSheet& s,const std::string& id){
        auto i=std::find_if(s.frames.begin(),s.frames.end(),[&](const auto& f){
            return f.id==id;
        });
        return i==s.frames.end()?nullptr:&*i;
    }
}
double SpriteClipDuration(const SpriteClip& c){
    double t=0;
    for(const auto& f:c.frames)t+=f.seconds;
    return t;
}
const SpriteClip* FindSpriteClip(const SpriteSheet& s,const std::string& name){
    auto i=std::find_if(s.clips.begin(),s.clips.end(),[&](const auto& c){
        return c.name==name;
    });
    return i==s.clips.end()?nullptr:&*i;
}
bool ValidSpriteSheet(const SpriteSheet& s,std::string& error){
    auto fail=[&](const std::string& e){
        error=e;
        return false;
    };
    if(!IsValidAssetId(s.texture)||s.width<=0||s.height<=0||s.width>32768||s.height>32768||!std::isfinite(s.pixelsPerUnit)||s.pixelsPerUnit<=0||s.pixelsPerUnit>1e6f)return fail("sprite sheet requires texture identity, bounded dimensions and pixels per unit");
    if(s.frames.empty()||s.frames.size()>4096||s.clips.size()>256)return fail("sprite sheet requires 1..4096 frames and at most 256 clips");
    std::set<std::string> ids,names;
    for(const auto& f:s.frames){
        auto r=f.pixels;
        if(!Key(f.id)||!Key(f.name)||!ids.insert(f.id).second||r.x<0||r.y<0||r.z<=0||r.w<=0||r.z>s.width||r.w>s.height||r.x>s.width-r.z||r.y>s.height-r.w||!std::isfinite(f.pivot.x)||!std::isfinite(f.pivot.y)||f.pivot.x<0||f.pivot.y<0||f.pivot.x>1||f.pivot.y>1)return fail("invalid/duplicate sprite frame key or out-of-image rectangle: "+f.id);
    }
    for(const auto& c:s.clips){
        if(!Key(c.name)||!names.insert(c.name).second||c.frames.empty()||c.frames.size()>4096)return fail("clip needs a unique name and 1..4096 frames");
        for(const auto& f:c.frames){
            if(!ids.count(f.frame))return fail("clip "+c.name+" references missing frame "+f.frame);
            if(!std::isfinite(f.seconds)||f.seconds<.0001||f.seconds>3600)return fail("frame duration must be 0.0001..3600 seconds");
        }
        const auto duration=SpriteClipDuration(c);
        if(duration>86400)return fail("sprite clip duration exceeds one day");
        if(!ValidateAnimationMarkerTrack(duration,c.events,error))return false;
    }
    error.clear();
    return true;
}
std::string EncodeSpriteSheet(const SpriteSheet& s){
    Json j={
        {
            "version",1
        },{
            "texture",s.texture
        },{
            "width",s.width
        },{
            "height",s.height
        },{
            "pixelsPerUnit",s.pixelsPerUnit
        },{
            "frames",Json::array()
        },{
            "clips",Json::array()
        }
    };
    for(const auto& f:s.frames)j["frames"].push_back({
        {
            "id",f.id
        },{
            "name",f.name
        },{
            "pixels",{
                f.pixels.x,f.pixels.y,f.pixels.z,f.pixels.w
            }
        },{
            "pivot",{
                f.pivot.x,f.pivot.y
            }
        }
    });
    for(const auto& c:s.clips){
        Json a={
            {
                "name",c.name
            },{
                "frames",Json::array()
            },{
                "events",Json::array()
            }
        };
        for(const auto& f:c.frames)a["frames"].push_back({
            {
                "frame",f.frame
            },{
                "seconds",f.seconds
            }
        });
        for(const auto& e:c.events)a["events"].push_back({
            {
                "id",e.id
            },{
                "name",e.name
            },{
                "time",e.time
            },{
                "data",e.data
            }
        });
        j["clips"].push_back(a);
    }
    return j.dump(2)+"\n";
}
bool ParseSpriteSheet(const std::string& text,SpriteSheet& out,std::string& error){
    try{
        if(text.size()>4*1024*1024)throw std::runtime_error("sprite sheet exceeds 4 MiB");
        auto j=Json::parse(text,[](int depth,Json::parse_event_t,const Json&){
            if(depth>32)throw std::runtime_error("sprite JSON nesting bound");
            return true;
        });
        if(j.at("version")!=1)throw std::runtime_error("unsupported sprite sheet version");
        if(!j.at("frames").is_array()||j.at("frames").size()>4096||!j.at("clips").is_array()||j.at("clips").size()>256)throw std::runtime_error("sprite array bounds");
        for(const auto* key:{
            "width","height"
        })if(!j.at(key).is_number_integer()||j.at(key).get<int64_t>()<1||j.at(key).get<int64_t>()>32768)throw std::runtime_error("sprite image dimensions must be integers in 1..32768");
        SpriteSheet s;
        s.texture=j.at("texture");
        s.width=j.at("width");
        s.height=j.at("height");
        s.pixelsPerUnit=j.at("pixelsPerUnit");
        for(const auto& f:j.at("frames")){
            SpriteFrame v;
            v.id=f.at("id");
            v.name=f.at("name");
            auto p=f.at("pixels");
            if(p.size()!=4)throw std::runtime_error("frame pixels require four integers");
            for(int i=0;i<4;++i){
                if(!p[i].is_number_integer()||p[i].get<int64_t>()<0||p[i].get<int64_t>()>32768)throw std::runtime_error("frame pixels must be integers in 0..32768");
                v.pixels[i]=p[i].get<int>();
            }
            auto q=f.at("pivot");
            if(q.size()!=2)throw std::runtime_error("pivot requires two coordinates");
            v.pivot={
                q[0],q[1]
            };
            s.frames.push_back(v);
        }
        for(const auto& c:j.at("clips")){
            if(!c.at("frames").is_array()||c.at("frames").size()>4096||!c.at("events").is_array()||c.at("events").size()>1024)throw std::runtime_error("sprite clip array bounds");
            SpriteClip v;
            v.name=c.at("name");
            for(const auto& f:c.at("frames"))v.frames.push_back({
                f.at("frame"),f.at("seconds")
            });
            for(const auto& e:c.at("events")){
                SpriteFrameEvent a;
                a.id=e.at("id");
                a.name=e.at("name");
                a.time=e.at("time");
                a.data=e.value("data",std::string{
                });
                v.events.push_back(a);
            }
            s.clips.push_back(v);
        }
        if(!ValidSpriteSheet(s,error))return false;
        out=std::move(s);
        error.clear();
        return true;
    }
    catch(const std::exception& e){
        error=e.what();
        return false;
    }
}
bool LoadSpriteSheet(const std::string& path,SpriteSheet& out,std::string& error){
    std::error_code ec;
    if(std::filesystem::file_size(path,ec)>4*1024*1024||ec){
        error="sprite sheet is missing or exceeds 4 MiB: "+path;
        return false;
    }
    std::ifstream f(path,std::ios::binary);
    if(!f){
        error="cannot open sprite sheet: "+path;
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(f)),{
    });
    return ParseSpriteSheet(text,out,error);
}
bool SaveSpriteSheet(const std::string& path,const SpriteSheet& s,std::string& error){
    if(!ValidSpriteSheet(s,error))return false;
    // Replace only after a complete, validated write. Source image is never touched.
    std::string tmp;
    try{
        tmp=CreateImportStagingFile(path);
        {
            std::ofstream f(tmp,std::ios::binary|std::ios::trunc);
            f<<EncodeSpriteSheet(s);
            f.close();
            if(!f)throw std::runtime_error("cannot write sprite sheet temporary file");
        }
        ReplaceStagedFile(tmp,path);
        error.clear();
        return true;
    }
    catch(const std::exception& e){
        if(!tmp.empty()){
            std::error_code ec;
            std::filesystem::remove(tmp,ec);
        }
        error=e.what();
        return false;
    }
}
bool SliceSpriteSheet(SpriteSheet& sheet,glm::ivec2 cell,glm::ivec2 margin,glm::ivec2 spacing,std::string& error){
    if(cell.x<=0||cell.y<=0||margin.x<0||margin.y<0||spacing.x<0||spacing.y<0||cell.x>32768||cell.y>32768||spacing.x>32768||spacing.y>32768){
        error="invalid slice cell, margin or spacing";
        return false;
    }
    SpriteSheet s=sheet;
    s.frames.clear();
    for(int y=margin.y,row=0;y<=s.height-cell.y;y+=cell.y+spacing.y,++row)for(int x=margin.x,col=0;x<=s.width-cell.x;x+=cell.x+spacing.x,++col){
        if(s.frames.size()==4096){
            error="slice exceeds 4096 frames";
            return false;
        }
        SpriteFrame f;
        f.id="cell-"+std::to_string(row)+"-"+std::to_string(col);
        f.name=f.id;
        f.pixels={
            x,y,cell.x,cell.y
        };
        if(auto* old=Frame(sheet,f.id)){
            f.name=old->name;
            f.pivot=old->pivot;
        }
        s.frames.push_back(f);
    }
    if(!ValidSpriteSheet(s,error))return false;
    sheet=std::move(s);
    return true;
}
bool ValidSpritePlayback(const SpritePlayback& p){
    return !p.sheet.empty()&&p.sheet.size()<=256&&p.clip.size()<=128&&p.frame.size()<=128&&std::isfinite(p.time)&&std::abs(p.time)<=1e9&&std::isfinite(p.speed)&&std::abs(p.speed)<=100;
}
std::string EncodeSpritePlayback(const SpritePlayback& p){
    return Json{
        {
            "version",1
        },{
            "sheet",p.sheet
        },{
            "clip",p.clip
        },{
            "frame",p.frame
        },{
            "time",p.time
        },{
            "speed",p.speed
        },{
            "playing",p.playing
        },{
            "loop",p.loop
        }
    }.dump();
}
bool DecodeSpritePlayback(const std::string& text,SpritePlayback& out,std::string& error){
    try{
        if(text.size()>4096)throw std::runtime_error("sprite playback exceeds 4096 bytes");
        auto j=Json::parse(text,[](int depth,Json::parse_event_t,const Json&){
            if(depth>32)throw std::runtime_error("sprite JSON nesting bound");
            return true;
        });
        if(j.at("version")!=1)throw std::runtime_error("unsupported sprite playback version");
        SpritePlayback p;
        p.sheet=j.at("sheet");
        p.clip=j.at("clip");
        p.frame=j.at("frame");
        p.time=j.at("time");
        p.speed=j.at("speed");
        p.playing=j.at("playing");
        p.loop=j.at("loop");
        if(!ValidSpritePlayback(p))throw std::runtime_error("invalid sprite playback");
        out=std::move(p);
        error.clear();
        return true;
    }
    catch(const std::exception& e){
        error=e.what();
        return false;
    }
}
bool SampleSpriteFrame(const SpriteSheet& s,const SpritePlayback& p,SpriteSettings& sprite,std::string& error,std::string* key){
    std::string id=p.frame;
    if(!p.clip.empty()){
        auto* c=FindSpriteClip(s,p.clip);
        if(!c){
            error="unknown sprite clip: "+p.clip;
            return false;
        }
        if(c->frames.empty()||c->frames.size()>4096){error="sprite clip frame count is invalid";return false;}
        const auto duration=SpriteClipDuration(*c);
        if(!std::isfinite(duration)||duration<=0||duration>86400){error="sprite clip duration is invalid";return false;}
        double t=p.loop?p.time-std::floor(p.time/duration)*duration:std::clamp(p.time,0.0,duration);
        id=c->frames.back().frame;
        double end=0;
        for(const auto& f:c->frames){
            end+=f.seconds;
            if(t<end){
                id=f.frame;
                break;
            }
        }
    }
    else if(id.empty()&&!s.frames.empty())id=s.frames.front().id;
    const auto* f=Frame(s,id);
    if(!f){
        error="missing sprite frame: "+id;
        return false;
    }
    SpriteSettings candidate=sprite;
    candidate.rect=glm::vec4(f->pixels)/glm::vec4(s.width,s.height,s.width,s.height);
    candidate.size=glm::vec2(f->pixels.z,f->pixels.w)/s.pixelsPerUnit;
    candidate.pivot=f->pivot;
    // Sprite samplers do not use mipmaps. For smooth sampling, constrain the
    // footprint to texel centres of this frame so adjacent atlas cells cannot bleed.
    if(!candidate.nearest){
        candidate.rect.x+=.5f/s.width;
        candidate.rect.y+=.5f/s.height;
        candidate.rect.z=std::max(.000001f,float(f->pixels.z-1))/s.width;
        candidate.rect.w=std::max(.000001f,float(f->pixels.w-1))/s.height;
    }
    if(!ValidSprite(candidate)){
        error="invalid sampled sprite geometry";
        return false;
    }
    sprite=candidate;
    if(key)*key=id;
    error.clear();
    return true;
}
bool StepSpriteAnimation(const SpriteSheet& s,SpritePlayback& p,double dt,SpriteAnimationStep& step){
    step={
    };
    if(!ValidSpritePlayback(p)||!std::isfinite(dt)||dt<0||dt>86400){
        step.error="invalid sprite playback step";
        return false;
    }
    SpriteSettings sampled;
    if(!SampleSpriteFrame(s,p,sampled,step.error))return false;
    if(!p.playing||p.clip.empty()||dt==0||p.speed==0)return true;
    const auto& c=*FindSpriteClip(s,p.clip);
    const auto duration=SpriteClipDuration(c);
    const auto to=p.time+dt*p.speed;
    step.events=CollectAnimationMarkers<SpriteFrameEventHit>(c.name,duration,c.events,p.time,to,p.loop,step.overflow);
    if(step.overflow){
        step.error="sprite event interval exceeds bounded policy; no time or partial events published";
        return false;
    }
    auto candidate=p;
    candidate.time=p.loop?to:std::clamp(to,0.0,duration);
    if(!ValidSpritePlayback(candidate)){
        step.error="sprite playback time bound exceeded";
        step.events.clear();
        return false;
    }
    step.finished=!p.loop&&(p.speed>0?to>=duration:to<=0);
    if(step.finished)candidate.playing=false;
    p=candidate;
    return true;
}
