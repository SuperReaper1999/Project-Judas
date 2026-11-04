#include "EditorCreationMenu.h"
#include "AssetBrowserModel.h"
#include "AssetBrowserPanel.h"
#include "EditorAssetActions.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorScriptWorkflow.h"
#include "Project.h"
#include "SpriteSheetEditor.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>

using namespace EditorPanelWidgets;
namespace {
void SelectAsset(const AssetRecord &record, EditorPanelState &state, const AssetDatabase &db) {
    state.browserSelection = record.id;
    state.moveAssetInput = record.path.size() > db.AssetsDir().size() + 1 &&
                                   record.path.compare(0, db.AssetsDir().size(), db.AssetsDir()) == 0
                               ? record.path.substr(db.AssetsDir().size() + 1)
                               : record.relativePath;
}
void OpenAsset(const AssetRecord &record, EditorPanelState &state) {
    const auto &id = record.id;
    const auto name = std::filesystem::path(record.relativePath).filename().string();
    if (record.type == AssetType::SpriteSheet) {
        state.spriteSheetEdit = id;
        state.showSpriteSheetEditor = true;
    }
    const bool aiDefinition = record.type == AssetType::Script && name.size() >= 6 &&
                              name.compare(name.size() - 6, 6, ".ai.js") == 0;
    if (aiDefinition) {
        state.utilityAIEdit = id;
        state.showUtilityAI = true;
        state.utilityAIAuthorRequested = true;
        state.utilityAILiveRequested = false;
    } else if (!RequestEditorScriptEdit(record, state))
        state.selectAssetDetails = true;
}
void DrawAssetInteraction(const AssetRecord &record, EditorDocument &doc, EditorPanelState &state,
                          EditorRequests &requests, const AssetDatabase &db) {
    const auto &id = record.id;
    const auto selectAsset = [&] { SelectAsset(record, state, db); };
    if (!record.missing && ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(kAssetDragPayload, id.data(), id.size());
        ImGui::Text("%s (%s)", record.relativePath.c_str(), AssetTypeName(record.type));
        ImGui::TextDisabled(record.type == AssetType::Mesh
                                ? "Drop on the viewport to place, or on a Mesh field."
                                : "Drop on a matching inspector field.");
        ImGui::EndDragDropSource();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\nID %s\nSource %s\n%s", record.path.c_str(), id.c_str(), record.source.c_str(),
                          record.type == AssetType::Script
                              ? "Double-click to edit script; right-click for details."
                              : "Double-click to inspect; drag to a matching field.");
    if (ImGui::BeginPopupContextItem("##assetActions")) {
        if (record.type == AssetType::Texture &&
            ImGui::MenuItem("Create/edit sprite sheet", nullptr, false,
                            state.mode == EditorMode::Edit && !record.missing && !record.readOnly)) {
            AssetRecord sheet;
            std::string error;
            if (CreateSpriteSheetAsset(*state.assets, id, sheet, error)) {
                state.spriteSheetEdit = sheet.id;
                state.showSpriteSheetEditor = true;
            } else
                state.status = error;
        }
        if (record.type == AssetType::SpriteSheet &&
            ImGui::MenuItem("Paint tilemap from this sheet", nullptr, false,
                            state.mode == EditorMode::Edit && !record.missing)) {
            state.tilemapPalette = id;
            state.showTilemapEditor = true;
        }
        if (record.type == AssetType::SpriteSheet && ImGui::MenuItem("Edit frames and animation")) {
            state.spriteSheetEdit = id;
            state.showSpriteSheetEditor = true;
        }
        if (record.type == AssetType::Script &&
            ImGui::MenuItem("Open as AI definition", nullptr, false, !record.missing)) {
            state.utilityAIEdit = id;
            state.showUtilityAI = true;
            state.utilityAIAuthorRequested = true;
            state.utilityAILiveRequested = false;
        }

        if (record.type == AssetType::Texture &&
            ImGui::MenuItem("Create sprite in scene", nullptr, false,
                            state.mode == EditorMode::Edit && !record.missing)) {
            std::string error;
            const auto id = CreateEditorSpriteFromImage(doc, db, record.id, state.cameraFocus, 0, error);
            state.status = id ? "Sprite created at 32 pixels per world unit; edit size, pivot and "
                                "sorting in Sprite."
                              : error;
        }
        if (record.type == AssetType::Script &&
            ImGui::MenuItem("Edit script", nullptr, false, !record.missing && !record.readOnly))
            RequestEditorScriptEdit(record, state);
        if (record.type == AssetType::Script &&
            ImGui::MenuItem("Attach to selected object", nullptr, false,
                            state.mode == EditorMode::Edit && doc.Selection().size() == 1 &&
                                !record.missing)) {
            std::string error;
            state.status = AttachEditorScript(doc, doc.Selected(), db, id, error)
                               ? "Script attached; save scene, then Play"
                               : error;
        }
        if (ImGui::MenuItem("Inspect asset")) {
            selectAsset();
            state.selectAssetDetails = true;
        }
        if (ImGui::MenuItem("Open file externally", nullptr, false, !record.missing))
            requests.openExternalPath = record.path;
        if (ImGui::MenuItem("Copy file path"))
            ImGui::SetClipboardText(record.path.c_str());
        if (ImGui::MenuItem("Copy stable ID"))
            ImGui::SetClipboardText(id.c_str());
        ImGui::EndPopup();
    }
}
void DrawResourceState(const AssetRecord &record, EditorPanelState &state) {
    const auto &id = record.id;
    if (record.missing) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "MISSING");
    } else if (state.resources) {
        // Milestone 31: the live resource state behind the asset.
        const ResourceState rs = state.resources->StateOf(id);
        switch (rs) {
        case ResourceState::Queued:
        case ResourceState::Loading:
        case ResourceState::CpuReady:
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "loading");
            break;
        case ResourceState::Ready:
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "ready");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Resident %.1f KB",
                                  static_cast<double>(state.resources->BytesOf(id)) / 1024.0);
            break;
        case ResourceState::Failed:
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "FAILED");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", state.resources->ErrorOf(id).c_str());
            break;
        default:
            ImGui::TextDisabled("registered");
            break;
        }
    } else {
        ImGui::TextDisabled("ok");
    }
}

} // namespace

void DrawAssetList(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests,
                   const AssetDatabase &db) {
    if (!state.assetBrowser)
        state.assetBrowser = std::make_shared<EditorAssetBrowserState>();
    auto &browser = *state.assetBrowser;
    const auto projectRoot = state.project ? state.project->RootDir() : db.AssetsDir();
    if (browser.projectRoot != projectRoot) {
        browser.projectRoot = projectRoot;
        browser.directory.clear();
    }
    ImGui::BeginChild("assets", {0, 0});
    char assetSearch[256];
    CopyToBuffer(state.assetSearch, assetSearch, sizeof(assetSearch));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##assetSearch", "Search assets, folders, IDs and sources...", assetSearch,
                                 sizeof(assetSearch)))
        state.assetSearch = assetSearch;
    ImGuiTextFilter searchFilter(assetSearch);
    ImGui::BeginDisabled(state.mode != EditorMode::Edit);
    if (ImGui::Button("Import assets..."))
        requests.browseImport = true;
    ImGui::EndDisabled();
    NextActionOnRow("Filters...");
    if (ImGui::Button("Filters..."))
        ImGui::OpenPopup("assetFilters");
    if (ImGui::BeginPopup("assetFilters")) {
        const char *assetTypes[] = {
            "All types",   "Mesh",  "Texture",      "Font",       "Audio",     "Prefab",
            "Script",      "UI",    "Navigation",   "Liquid",     "Material",  "Environment",
            "Catalog",     "World", "Audio effect", "Deformable", "Collision", "Physical material",
            "Sprite sheet"};
        int selectedType = state.assetTypeFilter + 1;
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::Combo("##assetType", &selectedType, assetTypes, 19))
            state.assetTypeFilter = selectedType - 1;
        ImGui::Checkbox("Missing only", &state.assetMissingOnly);
        ImGui::Checkbox("Include subfolders", &browser.recursive);
        ImGui::EndPopup();
    }
    NextActionOnRow("Rescan");
    if (ImGui::SmallButton("Rescan"))
        requests.rescanAssets = true;
    ToolbarHint("Refresh registered, missing and untracked project files");
    NextActionOnRow(browser.grid ? "List view" : "Grid view");
    if (ImGui::SmallButton(browser.grid ? "List view" : "Grid view"))
        browser.grid = !browser.grid;
    if (const auto *selected = db.Find(state.browserSelection);
        selected && selected->type == AssetType::Script) {
        ImGui::BeginDisabled(selected->missing || selected->readOnly);
        if (ImGui::Button("Edit script"))
            RequestEditorScriptEdit(*selected, state);
        ImGui::EndDisabled();
        NextActionOnRow("Attach to selected object");
        ImGui::BeginDisabled(state.mode != EditorMode::Edit || doc.Selection().size() != 1 ||
                             selected->missing);
        if (ImGui::Button("Attach to selected object")) {
            std::string error;
            state.status = AttachEditorScript(doc, doc.Selected(), db, selected->id, error)
                               ? "Script attached; save scene, then Play"
                               : error;
        }
        ImGui::EndDisabled();
        ToolbarHint(
            "Select one scene object, then attach this script. This is a normal undoable scene edit.");
    }

    const auto directories = EditorAssetBrowserDirectories(db);
    if (!browser.directory.empty() &&
        std::find(directories.begin(), directories.end(), browser.directory) == directories.end())
        browser.directory.clear();
    if (ImGui::BeginTable("Asset navigation", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed,
                                155 * ImGui::GetIO().FontGlobalScale);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
        ImGui::BeginChild("Asset folders", {0, 0});
        if (ImGui::Selectable("Assets", browser.directory.empty())) {
            browser.directory.clear();
            state.assetSearch.clear();
        }
        ImGui::SetItemTooltip("The tree contains registered asset folders. Import or track files to "
                              "make them referenceable.");
        for (const auto &directory : directories) {
            if (directory.empty())
                continue;
            ImGui::PushID(directory.c_str());
            const auto depth = std::count(directory.begin(), directory.end(), '/');
            const auto indent = float(depth) * 12 * ImGui::GetIO().FontGlobalScale;
            if (indent)
                ImGui::Indent(indent);
            const auto label = std::filesystem::path(directory).filename().string();
            if (ImGui::Selectable(label.c_str(), browser.directory == directory)) {
                browser.directory = directory;
                state.assetSearch.clear();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", directory.c_str());
            if (indent)
                ImGui::Unindent(indent);
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("Assets")) {
            browser.directory.clear();
            state.assetSearch.clear();
        }
        std::string path;
        for (const auto &part : std::filesystem::path(browser.directory)) {
            path += (path.empty() ? "" : "/") + part.generic_string();
            ImGui::SameLine();
            ImGui::TextDisabled("/");
            ImGui::SameLine();
            ImGui::PushID(path.c_str());
            if (ImGui::SmallButton(part.generic_string().c_str())) {
                browser.directory = path;
                state.assetSearch.clear();
            }
            ImGui::PopID();
        }
        const bool searching = !state.assetSearch.empty();
        if (searching)
            ImGui::TextDisabled("Search results across all folders");
        auto records = EditorAssetBrowserRecords(db, searching ? std::string{} : browser.directory,
                                                 searching || browser.recursive, state.assetTypeFilter,
                                                 state.assetMissingOnly);
        records.erase(std::remove_if(records.begin(), records.end(),
                                     [&](const auto *record) {
                                         const auto searchable =
                                             record->relativePath + " " + record->id + " " + record->source;
                                         return !searchFilter.PassFilter(searchable.c_str());
                                     }),
                      records.end());
        ImGui::TextDisabled("%zu asset%s", records.size(), records.size() == 1 ? "" : "s");
        if (records.empty() && (searching || directories.size() == 1))
            ImGui::TextWrapped(
                "No matching assets. Choose another folder, clear search/filters, or import files.");
        const float tileWidth = 112 * ImGui::GetIO().FontGlobalScale;
        const int columns = browser.grid ? std::clamp(int(ImGui::GetContentRegionAvail().x /
                                                          (tileWidth + ImGui::GetStyle().ItemSpacing.x)),
                                                      1, 64)
                                         : 3;
        if (ImGui::BeginTable(browser.grid ? "assetGrid" : "assetTable", columns,
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                                  (browser.grid ? ImGuiTableFlags_None
                                                : ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV),
                              {0, std::max(35.f, ImGui::GetContentRegionAvail().y)})) {
            if (!browser.grid) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 85);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 85);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
            }
            // Folder tiles mirror the tree; double-click enters a folder, and
            // breadcrumbs/tree offer single-click navigation without a modal.
            if (!searching) {
                for (const auto &directory : directories) {
                    if (directory.empty() ||
                        std::filesystem::path(directory).parent_path().generic_string() != browser.directory)
                        continue;
                    ImGui::TableNextColumn();
                    ImGui::PushID(directory.c_str());
                    const auto name = std::filesystem::path(directory).filename().string();
                    const auto label = "[Folder] " + name;
                    if (ImGui::Selectable(label.c_str(), false,
                                          ImGuiSelectableFlags_AllowDoubleClick |
                                              (browser.grid ? 0 : ImGuiSelectableFlags_SpanAllColumns),
                                          browser.grid
                                              ? ImVec2(std::max(40.f, ImGui::GetContentRegionAvail().x),
                                                       86 * ImGui::GetIO().FontGlobalScale)
                                              : ImVec2{})) {
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            browser.directory = directory;
                            state.assetSearch.clear();
                        }
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Double-click to open %s", directory.c_str());
                    ImGui::PopID();
                    if (!browser.grid) {
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("Folder");
                        ImGui::TableNextColumn();
                    }
                }
            }
            for (const auto *item : records) {
                const auto &record = *item;
                ImGui::TableNextColumn();
                ImGui::PushID(record.id.c_str());
                const auto name = std::filesystem::path(record.relativePath).filename().string();
                const bool selected = state.browserSelection == record.id;
                const auto origin = ImGui::GetCursorScreenPos();
                const ImVec2 tile{std::max(40.f, ImGui::GetContentRegionAvail().x),
                                  86 * ImGui::GetIO().FontGlobalScale};
                if (ImGui::Selectable(browser.grid ? "##assetTile" : name.c_str(), selected,
                                      ImGuiSelectableFlags_AllowDoubleClick |
                                          (browser.grid ? 0 : ImGuiSelectableFlags_SpanAllColumns),
                                      browser.grid ? tile : ImVec2{})) {
                    SelectAsset(record, state, db);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        OpenAsset(record, state);
                }
                DrawAssetInteraction(record, doc, state, requests, db);
                if (browser.grid) {
                    auto *draw = ImGui::GetWindowDrawList();
                    const auto color =
                        ImGui::GetColorU32(record.type == AssetType::Script ? ImVec4(.85f, .45f, .78f, 1)
                                                                            : ImVec4(.25f, .75f, .74f, 1));
                    const ImVec2 badgeMin{origin.x + 8, origin.y + 5},
                        badgeMax{origin.x + tile.x - 8, origin.y + tile.y - 28};
                    draw->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(ImGuiCol_FrameBg), 5);
                    draw->AddRect(badgeMin, badgeMax, color, 5);
                    const auto *type = AssetTypeName(record.type);
                    const auto typeSize = ImGui::CalcTextSize(type);
                    draw->AddText({origin.x + (tile.x - typeSize.x) * .5f, origin.y + 18}, color, type);
                    draw->PushClipRect({origin.x + 4, origin.y + tile.y - 25},
                                       {origin.x + tile.x - 4, origin.y + tile.y}, true);
                    draw->AddText({origin.x + 6, origin.y + tile.y - 24}, ImGui::GetColorU32(ImGuiCol_Text),
                                  name.c_str());
                    draw->PopClipRect();
                }
                ImGui::PopID();
                if (!browser.grid) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(AssetTypeName(record.type));
                    ImGui::TableNextColumn();
                    DrawResourceState(record, state);
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}
