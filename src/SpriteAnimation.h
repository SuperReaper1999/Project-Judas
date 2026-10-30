#pragma once
#include "Sprite.h"
#include "AnimationEventIntervals.h"
#include <vector>

// Stable frame keys are distinct from display names and clip indices.
struct SpriteFrame {
    std::string id, name;
    glm::ivec4 pixels{0,0,32,32}; // top-left origin
    glm::vec2 pivot{.5f};
};
struct SpriteClipFrame { std::string frame; double seconds=.1; };
struct SpriteFrameEvent {std::string id,name,data;double time=0;};
struct SpriteFrameEventHit {SpriteFrameEvent event;std::string clip;double crossing=0;bool reverse=false;};
struct SpriteClip {
    std::string name;
    std::vector<SpriteClipFrame> frames;
    std::vector<SpriteFrameEvent> events;
};
struct SpriteSheet {
    std::string texture;
    int width=32,height=32;
    float pixelsPerUnit=32;
    std::vector<SpriteFrame> frames;
    std::vector<SpriteClip> clips;
};
struct SpritePlayback {
    std::string sheet,clip,frame; // frame selects a still when clip is empty
    double time=0;
    float speed=1;
    bool playing=true,loop=true;
};
bool ValidSpriteSheet(const SpriteSheet&,std::string& error);
bool ParseSpriteSheet(const std::string&,SpriteSheet&,std::string& error);
bool LoadSpriteSheet(const std::string& path,SpriteSheet&,std::string& error);
std::string EncodeSpriteSheet(const SpriteSheet&);
bool SaveSpriteSheet(const std::string& path,const SpriteSheet&,std::string& error);
// Grid IDs are coordinate keys, never array positions. Clips referencing removed
// keys make validation fail; re-slicing cannot silently retarget a clip.
bool SliceSpriteSheet(SpriteSheet&,glm::ivec2 cell,glm::ivec2 margin,glm::ivec2 spacing,std::string& error);
bool ValidSpritePlayback(const SpritePlayback&);
std::string EncodeSpritePlayback(const SpritePlayback&);
bool DecodeSpritePlayback(const std::string&,SpritePlayback&,std::string& error);
double SpriteClipDuration(const SpriteClip&);
const SpriteClip* FindSpriteClip(const SpriteSheet&,const std::string&);
bool SampleSpriteFrame(const SpriteSheet&,const SpritePlayback&,SpriteSettings&,std::string& error,std::string* key=nullptr);
struct SpriteAnimationStep {
    std::vector<SpriteFrameEventHit> events;
    bool finished=false,overflow=false;
    std::string error;
};
// Atomic on invalid input/event overflow. Seeks use sampling, never this method.
bool StepSpriteAnimation(const SpriteSheet&,SpritePlayback&,double dt,SpriteAnimationStep&);
