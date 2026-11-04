#include "EditorToolPanels.h"
#include "JudasToolsPanel.h"
#include "EditorContext.h"
#include "EditorPanelRegistry.h"
#include "EditorPanels.h"
#include "EditorWorkspace.h"
#include "ScriptTools.h"
#include "TerrainTools.h"
#include "VolumeTerrainTools.h"
#include "ModTools.h"
#include "AnimationTimeline.h"
#include "UtilityAIEditor.h"
#include "SpriteSheetEditor.h"
#include "TilemapEditor.h"
#include "EngineHost.h"
#include "AssetDatabase.h"
#include "EditorTheme.h"
#include "imgui.h"

void PumpEditorScriptTools(EditorContext &c, std::unique_ptr<ScriptTools> &scripts) {
    if (!scripts)
        scripts = std::make_unique<ScriptTools>();
    if (!c.panels.editScriptAsset.empty()) {
        const auto *asset = c.assets.Find(c.panels.editScriptAsset);
        c.panels.showScriptTools = true;
        if (!asset || !scripts->OpenForEditing(asset->relativePath, c.assets))
            c.panels.status = scripts->status;
        c.panels.editScriptAsset.clear();
    }
}
void RegisterEditorToolPanels(EditorPanelRegistry &registry, EditorToolServices tools) {
    constexpr unsigned author = EditorPhaseMask(EditorPanelPhase::Edit);
    constexpr unsigned live =
        author | EditorPhaseMask(EditorPanelPhase::Play) | EditorPhaseMask(EditorPanelPhase::Paused);
    registry.Add(
        {"scripts", "JudasJS developer tools", "Scripting", live, &EditorPanelState::showScriptTools},
        [tools](auto &c) {
            PumpEditorScriptTools(c, tools.scripts);
            tools.scripts->visible = c.panels.showScriptTools;
            tools.scripts->Panel(c.runtime, c.assets);
            c.panels.showScriptTools = tools.scripts->visible;
        });
    registry.Add({"terrain", "Terrain tools", "World", author, &EditorPanelState::showTerrain},
                 [tools](auto &c) {
                     if (!tools.terrain)
                         tools.terrain = std::make_unique<TerrainEditor>();
                     tools.terrain->Panel(c.document, c.panels, c.requests);
                 });
    registry.Add({"volume", "Volumetric terrain", "World", author, &EditorPanelState::showVolumeTerrain},
                 [tools](auto &c) {
                     if (!tools.volume)
                         tools.volume = std::make_unique<VolumeTerrainEditor>();
                     tools.volume->Panel(c.document, c.panels, c.requests);
                 });
    registry.Add({"mods", "Local mods", "Project", author, &EditorPanelState::showMods}, [tools](auto &c) {
        if (!tools.mods)
            tools.mods = std::make_unique<ModTools>();
        tools.mods->Panel(c.panels, c.requests);
    });
    registry.Add(
        {"animation", "Animation timeline", "Animation", live, &EditorPanelState::showAnimationTimeline},
        [tools](auto &c) {
            if (!tools.animation)
                tools.animation = std::make_unique<AnimationTimeline>();
            tools.animation->visible = c.panels.showAnimationTimeline;
            tools.animation->Panel(c.panels, c.document.Selected());
            c.panels.showAnimationTimeline = tools.animation->visible;
        });
    registry.Add({"ai", "AI actions and decisions", "AI", live, &EditorPanelState::showUtilityAI},
                 [tools](auto &c) {
                     if (!tools.ai)
                         tools.ai = std::make_unique<UtilityAIEditor>();
                     c.panels.utilityAISelected = c.document.Selected();
                     tools.ai->Panel(c.panels);
                 });
    registry.Add({"sprites", "Sprite sheets and frame animation", "Animation", author,
                  &EditorPanelState::showSpriteSheetEditor},
                 [tools](auto &c) {
                     if (!tools.sprites)
                         tools.sprites = std::make_unique<SpriteSheetEditor>();
                     tools.sprites->Panel(c.panels);
                 });
    registry.Add({"tilemaps", "Tilemap painter", "World", author, &EditorPanelState::showTilemapEditor},
                 [tools](auto &c) {
                     if (!tools.tilemaps)
                         tools.tilemaps = std::make_unique<TilemapEditor>();
                     tools.tilemaps->Panel(c.document, c.panels);
                 });
    auto launcher = std::make_shared<JudasToolsPanel>();
    registry.Add({"judas-tools", "Judas Tools", "Tools", live, &EditorPanelState::showJudasTools},
                 [tools, launcher](auto &context) { launcher->Draw(context, tools.scripts); });
}
