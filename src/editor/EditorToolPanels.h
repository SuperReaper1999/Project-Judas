#pragma once
#include <memory>
class EditorPanelRegistry;
struct EditorContext;
struct ScriptTools;
struct TerrainEditor;
struct VolumeTerrainEditor;
struct ModTools;
struct AnimationTimeline;
struct UtilityAIEditor;
struct SpriteSheetEditor;
struct TilemapEditor;
struct EditorToolServices {
    std::unique_ptr<ScriptTools> &scripts;
    std::unique_ptr<TerrainEditor> &terrain;
    std::unique_ptr<VolumeTerrainEditor> &volume;
    std::unique_ptr<ModTools> &mods;
    std::unique_ptr<AnimationTimeline> &animation;
    std::unique_ptr<UtilityAIEditor> &ai;
    std::unique_ptr<SpriteSheetEditor> &sprites;
    std::unique_ptr<TilemapEditor> &tilemaps;
};
void RegisterEditorToolPanels(EditorPanelRegistry &, EditorToolServices);
void PumpEditorScriptTools(EditorContext &, std::unique_ptr<ScriptTools> &);
