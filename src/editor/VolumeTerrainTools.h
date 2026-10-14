#pragma once
#include "VolumeTerrain.h"
#include "TerrainTools.h"
struct VolumeTerrainEditor {
 VolumeTerrainDraft draft;VolumeBrush brush;std::string projectKey,error;
 bool loaded=false,editing=false;char path[512]="Imports/terrain.judasvolume";
 int resolution=33;glm::vec3 extent{16},point{0};float depth=0;
 std::shared_ptr<MeshData> preview;uint64_t previewRevision=~uint64_t(0);MeshHandle mesh;DebugLineList footprint;
 std::vector<NavigationDependency> navigationDependencies;
 struct PreviewTask {JobHandle job;uint64_t revision=0;std::string meshId;MeshData result;};
 std::shared_ptr<PreviewTask> previewTask;glm::vec2 editButtonCenter{};
 std::shared_ptr<TerrainEditor::Task> task;std::shared_ptr<TerrainEditor::NavigationTask> navigationTask;
 bool Busy(const EditorPanelState&)const;bool Guard(EditorPanelState&);
 void Panel(EditorDocument&,EditorPanelState&,EditorRequests&);
 void Prepare(Renderer&,const EditorPanelState&);void Shutdown(Renderer&,JobSystem*);
 bool Stroke(const Scene&,SceneObjectId,glm::vec3,glm::vec3,bool,bool,bool,bool,std::string&);
 void Draw(Renderer&,Scene&,ResourceManager&);
};
