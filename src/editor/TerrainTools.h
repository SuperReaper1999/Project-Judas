#pragma once
#include "TerrainAuthoring.h"
#include "EditorPanels.h"
#include "Renderer.h"
#include <memory>
struct TerrainEditor {
 TerrainDraft draft;TerrainBrush brush;
 std::string projectKey,error;bool loaded=false,editing=false,replaceRequested=false;
 // Editor smoke tests click the real primary control through SDL/ImGui.
 glm::vec2 editButtonCenter{};
 char path[512]="Imports/landscape.judasterrain";int resolution=65;float width=64,depth=64;
 std::shared_ptr<MeshData> preview;uint64_t previewRevision=~uint64_t(0);bool appearanceDirty=true;
 MeshHandle mesh;TextureHandle texture;DebugLineList footprint;uint64_t appearanceRevision=~uint64_t(0);
 struct Task {JobHandle job;TerrainCookResult result;};std::shared_ptr<Task> task;
 void Panel(EditorDocument&,EditorPanelState&,EditorRequests&);
 bool Busy(const EditorPanelState&)const;
 bool Guard(EditorPanelState&);
 void Shutdown(Renderer&,JobSystem*);
 void Prepare(Renderer&,const EditorPanelState&);
 bool Stroke(const Scene&,SceneObjectId,glm::vec3,glm::vec3,bool pressed,bool down,bool cancel,bool focused,std::string&);
 void Draw(Renderer&,Scene&,ResourceManager&);
};
