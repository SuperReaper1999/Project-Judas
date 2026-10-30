#include "Sprite.h"
#include "../third_party/nlohmann/json.hpp"

bool ValidSprite(const SpriteSettings& s) {
    for(float v:{s.size.x,s.size.y,s.pivot.x,s.pivot.y,s.rect.x,s.rect.y,s.rect.z,s.rect.w,s.tint.x,s.tint.y,s.tint.z,s.tint.w})if(!std::isfinite(v))return false;
    return s.size.x>0&&s.size.y>0&&s.size.x<=1e6f&&s.size.y<=1e6f&&
           s.pivot.x>=0&&s.pivot.x<=1&&s.pivot.y>=0&&s.pivot.y<=1&&
           s.rect.x>=0&&s.rect.y>=0&&s.rect.z>0&&s.rect.w>0&&s.rect.x+s.rect.z<=1&&s.rect.y+s.rect.w<=1&&
           s.sortingLayer>=-32768&&s.sortingLayer<=32767&&s.order>=-32768&&s.order<=32767&&
           glm::all(glm::greaterThanEqual(s.tint,glm::vec4(0)))&&glm::all(glm::lessThanEqual(s.tint,glm::vec4(1)));
}
std::string EncodeSprite(const SpriteSettings& s) {
    return nlohmann::ordered_json{{"version",1},{"size",{s.size.x,s.size.y}},{"pivot",{s.pivot.x,s.pivot.y}},
        {"rect",{s.rect.x,s.rect.y,s.rect.z,s.rect.w}},{"tint",{s.tint.x,s.tint.y,s.tint.z,s.tint.w}},{"flipX",s.flipX},{"flipY",s.flipY},{"nearest",s.nearest},
        {"pixelSnap",s.pixelSnap},{"alphaPick",s.alphaPick},{"ySort",s.ySort},{"sortingLayer",s.sortingLayer},{"order",s.order}}.dump();
}
bool DecodeSprite(const std::string& text,SpriteSettings& out,std::string& error) {
    try {
        if(text.size()>8192)throw std::runtime_error("sprite settings exceed 8 KiB");
        auto j=nlohmann::ordered_json::parse(text);
        if(!j.is_object()||j.size()!=13||j.at("version")!=1)throw std::runtime_error("expected sprite version 1 fields");
        SpriteSettings s;
        auto vector=[&](const char* key,float* p,size_t count){const auto& a=j.at(key);if(!a.is_array()||a.size()!=count)throw std::runtime_error("invalid sprite vector");for(size_t i=0;i<count;++i)p[i]=a[i].get<float>();};
        vector("size",&s.size.x,2);vector("pivot",&s.pivot.x,2);vector("rect",&s.rect.x,4);
        vector("tint",&s.tint.x,4);
        s.flipX=j.at("flipX").get<bool>();s.flipY=j.at("flipY").get<bool>();s.nearest=j.at("nearest").get<bool>();
        s.pixelSnap=j.at("pixelSnap").get<bool>();s.alphaPick=j.at("alphaPick").get<bool>();s.ySort=j.at("ySort").get<bool>();
        if(!j.at("sortingLayer").is_number_integer()||!j.at("order").is_number_integer())throw std::runtime_error("sprite sort keys must be integers");
        const auto layer=j.at("sortingLayer").get<int64_t>(),order=j.at("order").get<int64_t>();
        if(layer < -32768||layer>32767||order < -32768||order>32767)throw std::runtime_error("sprite sort keys outside -32768..32767");
        s.sortingLayer=int(layer);s.order=int(order);
        if(!ValidSprite(s))throw std::runtime_error("invalid sprite dimensions, pivot or image rectangle");
        out=s;error.clear();return true;
    }catch(const std::exception& e){error=std::string("sprite: ")+e.what();return false;}
}
