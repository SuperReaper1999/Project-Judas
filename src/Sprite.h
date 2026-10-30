#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <string>
#include <tuple>
#include <cstdint>

// World-space render primitive. Texture identity remains the render component's
// ordinary AssetId; no physics, scripts or private file paths live here.
struct SpriteSettings {
    glm::vec2 size{1}, pivot{.5f};
    glm::vec4 rect{0,0,1,1}; // normalized source-image rect, top-left origin
    glm::vec4 tint{1};
    bool flipX=false,flipY=false,nearest=true,pixelSnap=true,alphaPick=true,ySort=false;
    int sortingLayer=0,order=0;
};
inline bool operator==(const SpriteSettings& a,const SpriteSettings& b) {
    return a.size==b.size&&a.pivot==b.pivot&&a.rect==b.rect&&a.tint==b.tint&&a.flipX==b.flipX&&a.flipY==b.flipY&&
           a.nearest==b.nearest&&a.pixelSnap==b.pixelSnap&&a.alphaPick==b.alphaPick&&a.ySort==b.ySort&&
           a.sortingLayer==b.sortingLayer&&a.order==b.order;
}
bool ValidSprite(const SpriteSettings&);
std::string EncodeSprite(const SpriteSettings&);
bool DecodeSprite(const std::string&,SpriteSettings&,std::string& error);
inline auto SpriteSortKey(const SpriteSettings& sprite,float worldY,std::uint64_t identity) {
    return std::make_tuple(sprite.sortingLayer,sprite.order,sprite.ySort?-worldY:0.f,identity);
}

inline glm::vec2 SpriteCentre(const SpriteSettings& sprite) {
    return glm::vec2((.5f-sprite.pivot.x)*sprite.size.x,(sprite.pivot.y-.5f)*sprite.size.y)*
           glm::vec2(sprite.flipX?-1:1,sprite.flipY?-1:1);
}
inline glm::vec2 SpriteUV(const SpriteSettings& sprite,glm::vec2 local) {
    auto pixel=(local-SpriteCentre(sprite))/(sprite.size*glm::vec2(sprite.flipX?-1:1,sprite.flipY?-1:1))+.5f;
    return {sprite.rect.x+pixel.x*sprite.rect.z,1-sprite.rect.y-sprite.rect.w+pixel.y*sprite.rect.w};
}
