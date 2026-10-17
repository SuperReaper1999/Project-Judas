#pragma once
#include "SkeletalAnimation.h"
#include <string>
// Project-owned recipe editing; never mutates the immutable runtime asset.
struct AnimationEventDraft {
 std::string path,baseline,clip;float duration=0;std::vector<AnimationEvent> events,savedEvents;
 bool Dirty()const;
 bool Load(const std::string& projectRoot,const std::string& recipe,const AnimationClip&,std::string& error);
 bool Save(std::string& error);
};
