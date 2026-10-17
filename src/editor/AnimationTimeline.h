#pragma once
#include "authoring/AnimationEventAuthoring.h"
struct EditorPanelState;
struct AnimationTimeline {
 bool visible=false;AnimationEventDraft draft;std::string status,loadedRecipe,loadedClip,ownerProject;
 std::vector<std::vector<AnimationEvent>> undo;int selected=-1;unsigned serial=0;
 void Panel(EditorPanelState&,uint64_t selectedEntity=0);
 void Remember(){if(undo.size()>=64)undo.erase(undo.begin());undo.push_back(draft.events);}
};
