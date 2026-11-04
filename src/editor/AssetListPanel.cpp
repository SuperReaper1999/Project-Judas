#include "EditorCreationMenu.h"
#include "AssetBrowserModel.h"
#include "AssetBrowserPanel.h"
#include "EditorAssetActions.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorScriptWorkflow.h"
#include "EditorTheme.h"
#include "EditorVisuals.h"
#include "Project.h"
#include "SpriteSheetEditor.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "EditorDesign.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <map>

using namespace EditorPanelWidgets;
namespace {
void SelectAsset(const AssetRecord &record, EditorPanelState &state, const AssetDatabase &db) {
    state.browserSelection = record.id;
    state.revealModelStructure = false;
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

EditorVisuals::Icon AssetIcon(AssetType type) {
    using Icon = EditorVisuals::Icon;
    switch (type) {
    case AssetType::Mesh:
    case AssetType::Prefab:
    case AssetType::Collision:
        return Icon::Cube;
    case AssetType::Texture:
    case AssetType::SpriteSheet:
        return Icon::Image;
    case AssetType::Script:
        return Icon::Script;
    case AssetType::Audio:
    case AssetType::AudioEffect:
        return Icon::Audio;
    case AssetType::UI:
        return Icon::UI;
    case AssetType::World:
    case AssetType::Environment:
        return Icon::Globe;
    case AssetType::Material:
    case AssetType::PhysicalMaterial:
        return Icon::Light;
    default:
        return Icon::File;
    }
}

void DrawAssetCard(const std::string &name, const char *kind, EditorVisuals::Icon icon, ImVec2 minimum,
                   ImVec2 maximum, bool selected, bool missing = false, EditorAssetThumbnail thumbnail = {}) {
    auto *draw = ImGui::GetWindowDrawList();
    const float scale = ImGui::GetFontSize() / 15.f;
    const auto accent =
        icon == EditorVisuals::Icon::Script ? EditorSecondaryAccent() : EditorVisuals::AccentColor();
    const bool hovered = ImGui::IsItemHovered();
    const float rounding = 8 * scale;
    const float footer = 47 * scale;
    const auto canvasEnd = ImVec2(maximum.x, maximum.y - footer);
    draw->AddRectFilled({minimum.x + scale, minimum.y + 3 * scale},
                        {maximum.x + scale, maximum.y + 3 * scale}, IM_COL32(0, 0, 0, 28), rounding);
    draw->AddRectFilled(minimum, maximum,
                        ImGui::GetColorU32(EditorVisuals::SurfaceColor(hovered ? .07f : .045f)), rounding);
    draw->AddRectFilled(minimum, canvasEnd, ImGui::GetColorU32(EditorVisuals::SurfaceColor(.09f)), rounding,
                        ImDrawFlags_RoundCornersTop);
    if (thumbnail) {
        // The actual texture is borrowed for this frame. The browser never
        // copies pixels, creates GPU objects or guesses an image's aspect.
        const ImVec2 lo{minimum.x + 5 * scale, minimum.y + 5 * scale};
        const ImVec2 hi{canvasEnd.x - 5 * scale, canvasEnd.y - 5 * scale};
        const float zoom = std::min((hi.x - lo.x) / thumbnail.width, (hi.y - lo.y) / thumbnail.height);
        const ImVec2 size{thumbnail.width * zoom, thumbnail.height * zoom};
        const ImVec2 at{lo.x + (hi.x - lo.x - size.x) * .5f, lo.y + (hi.y - lo.y - size.y) * .5f};
        const float check = 8 * scale;
        for (float y = lo.y; y < hi.y; y += check)
            for (float x = lo.x; x < hi.x; x += check) {
                const bool alternate = (int((x - lo.x) / check) + int((y - lo.y) / check)) % 2;
                draw->AddRectFilled(
                    {x, y}, {std::min(x + check, hi.x), std::min(y + check, hi.y)},
                    ImGui::GetColorU32(EditorVisuals::SurfaceColor(alternate ? .065f : .025f)));
            }
        draw->AddImage(ImTextureID(thumbnail.image), at, {at.x + size.x, at.y + size.y}, {0, 1}, {1, 0});
    } else if (icon == EditorVisuals::Icon::Folder) {
        const float width = std::min(58.f * scale, (maximum.x - minimum.x) * .60f);
        const float height = width * .65f;
        const ImVec2 at{minimum.x + (maximum.x - minimum.x - width) * .5f,
                        minimum.y + (canvasEnd.y - minimum.y - height) * .5f};
        auto color = accent;
        color.w = .70f;
        draw->AddRectFilled({at.x, at.y - height * .13f}, {at.x + width * .43f, at.y + height * .3f},
                            ImGui::GetColorU32(color), 4 * scale);
        draw->AddRectFilled(at, {at.x + width, at.y + height}, ImGui::GetColorU32(color), 4 * scale);
        color.w = 1;
        draw->AddRectFilled({at.x, at.y + height * .18f}, {at.x + width, at.y + height},
                            ImGui::GetColorU32(color), 4 * scale);
    } else {
        const float iconSize = std::min(38.f * scale, (canvasEnd.y - minimum.y) * .60f);
        EditorVisuals::DrawIcon(draw, icon,
                                {minimum.x + (maximum.x - minimum.x - iconSize) * .5f,
                                 minimum.y + (canvasEnd.y - minimum.y - iconSize) * .5f},
                                iconSize, ImGui::GetColorU32(accent));
    }
    const float textY = canvasEnd.y + 6 * scale;
    const ImVec4 clip{minimum.x + 8 * scale, textY, maximum.x - 8 * scale, maximum.y - 17 * scale};
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), {clip.x, textY}, ImGui::GetColorU32(ImGuiCol_Text),
                  name.c_str(), nullptr, clip.z - clip.x, &clip);
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * .76f,
                  {minimum.x + 8 * scale, maximum.y - 14 * scale},
                  missing ? IM_COL32(245, 136, 94, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled),
                  missing ? "Missing file" : kind);
    if (selected) {
        draw->AddRect(minimum, maximum, ImGui::GetColorU32(accent), rounding, 0, 1.5f * scale);
        draw->AddRectFilled({minimum.x + 5 * scale, maximum.y - 3 * scale},
                            {maximum.x - 5 * scale, maximum.y - scale}, ImGui::GetColorU32(accent), scale);
    }
}

void ChangeDirectory(EditorAssetBrowserState &browser, EditorPanelState &state,
                     const std::string &directory) {
    browser.directory = directory;
    state.assetSearch.clear();
}

void DrawFolderTree(EditorAssetBrowserState &browser, EditorPanelState &state,
                    const std::vector<std::string> &directories) {
    std::map<std::string, std::vector<std::string>> children;
    for (const auto &directory : directories)
        if (!directory.empty())
            children[std::filesystem::path(directory).parent_path().generic_string()].push_back(directory);
    const float iconSize = ImGui::GetFontSize();
    std::function<void(const std::string &)> folder = [&](const std::string &directory) {
        const bool root = directory.empty();
        const bool hasChildren = children.find(directory) != children.end();
        const bool selected = directory == browser.directory;
        const bool ancestor = !browser.directory.empty() &&
                              (root || (browser.directory.size() > directory.size() &&
                                        browser.directory.compare(0, directory.size(), directory) == 0 &&
                                        browser.directory[directory.size()] == '/'));
        if (ancestor)
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        const auto label =
            root ? std::string("Assets") : std::filesystem::path(directory).filename().string();
        ImGui::PushID(root ? "##root" : directory.c_str());
        const bool open = EditorDesign::TreeRow(label.c_str(), selected, hasChildren, root, iconSize + 4);
        const auto minimum = ImGui::GetItemRectMin();
        EditorVisuals::DrawIcon(ImGui::GetWindowDrawList(), EditorVisuals::Icon::Folder,
                                {minimum.x + ImGui::GetStyle().FramePadding.x + ImGui::GetFontSize(),
                                 minimum.y + (ImGui::GetItemRectSize().y - iconSize) * .5f},
                                iconSize, ImGui::GetColorU32(EditorVisuals::AccentColor()));
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            ChangeDirectory(browser, state, directory);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", root ? "Registered project assets" : directory.c_str());
        if (open && hasChildren) {
            ImGui::Indent(14 * ImGui::GetIO().FontGlobalScale);
            for (const auto &child : children.at(directory))
                folder(child);
            ImGui::Unindent(14 * ImGui::GetIO().FontGlobalScale);
        }
        ImGui::PopID();
    };
    folder({});
}

} // namespace

void DrawAssetList(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests,
                   const AssetDatabase &db) {
    if (!state.assetBrowser)
        state.assetBrowser = std::make_shared<EditorAssetBrowserState>();
    auto &browser = *state.assetBrowser;
    const auto projectRoot = state.project ? state.project->RootDir() : db.AssetsDir();
    if (browser.projectRoot != projectRoot) {
        if (browser.thumbnails)
            browser.thumbnails->Reset();
        browser.projectRoot = projectRoot;
        browser.directory.clear();
    }
    const float scale = ImGui::GetFontSize() / 15.f;
    if (!browser.thumbnails)
        browser.thumbnails = std::make_shared<EditorAssetThumbnailDemand>();
    browser.thumbnails->Begin(state.resources);
    if (!browser.grid)
        browser.thumbnails->Reset();
    ImGui::BeginChild("assets", {0, 0});
    ImGui::BeginDisabled(state.mode != EditorMode::Edit);
    if (EditorVisuals::ActionButton("Import assets...", EditorVisuals::Icon::Import, true))
        requests.browseImport = true;
    ImGui::EndDisabled();
    ToolbarHint(
        "Import files into this project. Registered assets can be dragged into the scene or inspector.");
    NextActionOnRow("    Create");
    ImGui::BeginDisabled(state.mode != EditorMode::Edit);
    if (EditorVisuals::ActionButton("Create", EditorVisuals::Icon::Plus))
        ImGui::OpenPopup("createAsset");
    if (ImGui::BeginPopup("createAsset")) {
        const auto *selected = db.Find(state.browserSelection);
        if (ImGui::MenuItem("Sprite sheet from selected image", nullptr, false,
                            selected && selected->type == AssetType::Texture && !selected->missing &&
                                !selected->readOnly)) {
            AssetRecord sheet;
            std::string error;
            if (CreateSpriteSheetAsset(*state.assets, selected->id, sheet, error)) {
                state.spriteSheetEdit = sheet.id;
                state.showSpriteSheetEditor = true;
            } else
                state.status = error;
        }
        if (ImGui::MenuItem("Create script on selected object...", nullptr, false,
                            doc.Selection().size() == 1)) {
            state.showInspector = true;
            state.revealComponent = "Scripts";
            state.status = "In the selected object's Scripts section, choose Create script & attach.";
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Materials, audio and asset tools..."))
            state.selectModelImportTools = true;
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    NextActionOnRow("Filters");
    const bool filtering = state.assetTypeFilter >= 0 || state.assetMissingOnly || browser.recursive;
    if (EditorDesign::ActionButton(filtering ? "Filters *" : "Filters", EditorDesign::Tone::Quiet))
        ImGui::OpenPopup("assetFilters");
    if (ImGui::BeginPopup("assetFilters")) {
        const char *types[] = {
            "All types",   "Mesh",  "Texture",      "Font",       "Audio",     "Prefab",
            "Script",      "UI",    "Navigation",   "Liquid",     "Material",  "Environment",
            "Catalog",     "World", "Audio effect", "Deformable", "Collision", "Physical material",
            "Sprite sheet"};
        int type = state.assetTypeFilter + 1;
        ImGui::SetNextItemWidth(190 * scale);
        if (ImGui::Combo("Type", &type, types, 19))
            state.assetTypeFilter = type - 1;
        ImGui::Checkbox("Missing only", &state.assetMissingOnly);
        ImGui::Checkbox("Include subfolders", &browser.recursive);
        if (ImGui::Button("Reset filters")) {
            state.assetTypeFilter = -1;
            state.assetMissingOnly = false;
            browser.recursive = false;
        }
        ImGui::EndPopup();
    }
    NextActionOnRow("View...");
    if (EditorVisuals::ActionButton("View", EditorVisuals::Icon::Image))
        ImGui::OpenPopup("assetView");
    if (ImGui::BeginPopup("assetView")) {
        if (ImGui::MenuItem("Thumbnail grid", nullptr, browser.grid))
            browser.grid = true;
        if (ImGui::MenuItem("Detailed list", nullptr, !browser.grid))
            browser.grid = false;
        ImGui::Separator();
        ImGui::SetNextItemWidth(170 * scale);
        ImGui::SliderFloat("Preview size", &browser.tileSize, 88.f, 160.f, "%.0f");
        ImGui::Separator();
        if (ImGui::MenuItem("Selected asset details", nullptr, false,
                            db.Find(state.browserSelection) != nullptr))
            state.selectAssetDetails = true;
        if (ImGui::MenuItem("Rescan project files"))
            requests.rescanAssets = true;
        if (ImGui::MenuItem("Import and asset tools"))
            state.selectModelImportTools = true;
        ImGui::EndPopup();
    }
    ToolbarHint("Thumbnail size, detailed list and project rescan.");
    char search[256];
    CopyToBuffer(state.assetSearch, search, sizeof(search));
    const float rowEnd = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (rowEnd - ImGui::GetItemRectMax().x - ImGui::GetStyle().ItemSpacing.x >= 220 * scale)
        ImGui::SameLine();
    if (EditorDesign::Search("##assetSearch", "Search assets, IDs or source files...", search,
                             sizeof(search)))
        state.assetSearch = search;
    ImGuiTextFilter searchFilter(state.assetSearch.c_str());
    if (const auto *selected = db.Find(state.browserSelection);
        selected && selected->type == AssetType::Script) {
        ImGui::BeginDisabled(selected->missing || selected->readOnly);
        if (ImGui::SmallButton("Edit selected script"))
            RequestEditorScriptEdit(*selected, state);
        ImGui::EndDisabled();
        NextActionOnRow("Attach to selected object");
        ImGui::BeginDisabled(state.mode != EditorMode::Edit || doc.Selection().size() != 1 ||
                             selected->missing);
        if (ImGui::SmallButton("Attach to selected object")) {
            std::string error;
            state.status = AttachEditorScript(doc, doc.Selected(), db, selected->id, error)
                               ? "Script attached; save scene, then Play"
                               : error;
        }
        ImGui::EndDisabled();
        ToolbarHint("Select one scene object, then attach this script. Attachment supports Undo.");
    }
    ImGui::Separator();

    const auto directories = EditorAssetBrowserDirectories(db);
    if (!browser.directory.empty() &&
        std::find(directories.begin(), directories.end(), browser.directory) == directories.end())
        browser.directory.clear();
    if (ImGui::BeginTable("Asset navigation", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 160 * scale);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorVisuals::SurfaceColor(.012f));
        ImGui::BeginChild("Asset folders", {0, 0});
        ImGui::PushFont(EditorVisuals::HeadingFont());
        EditorVisuals::IconText(EditorVisuals::Icon::Folder, "Project", 15);
        ImGui::PopFont();
        ImGui::Spacing();
        DrawFolderTree(browser, state, directories);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
        if (ImGui::SmallButton("Assets"))
            ChangeDirectory(browser, state, {});
        std::string path;
        for (const auto &part : std::filesystem::path(browser.directory)) {
            path += (path.empty() ? "" : "/") + part.generic_string();
            ImGui::SameLine(0, 5 * scale);
            ImGui::TextDisabled(">");
            ImGui::SameLine(0, 5 * scale);
            ImGui::PushID(path.c_str());
            if (ImGui::SmallButton(part.generic_string().c_str()))
                ChangeDirectory(browser, state, path);
            ImGui::PopID();
        }
        ImGui::PopStyleColor();
        const bool searching = !state.assetSearch.empty();
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
        ImGui::SameLine();
        ImGui::TextDisabled("  %zu asset%s%s", records.size(), records.size() == 1 ? "" : "s",
                            searching ? " / all folders" : "");
        const float tileWidth = browser.tileSize * scale;
        const int columns = browser.grid ? std::clamp(int(ImGui::GetContentRegionAvail().x /
                                                          (tileWidth + ImGui::GetStyle().ItemSpacing.x)),
                                                      1, 64)
                                         : 3;
        const auto flags =
            ImGuiTableFlags_ScrollY | (browser.grid ? ImGuiTableFlags_None
                                                    : ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                                                          ImGuiTableFlags_BordersInnerV);
        if (ImGui::BeginTable(browser.grid ? "assetGrid" : "assetTable", columns, flags,
                              {0, std::max(35.f, ImGui::GetContentRegionAvail().y)})) {
            if (!browser.grid) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90 * scale);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 78 * scale);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
            }
            if (!searching) {
                for (const auto &directory : directories) {
                    if (directory.empty() ||
                        std::filesystem::path(directory).parent_path().generic_string() != browser.directory)
                        continue;
                    ImGui::TableNextColumn();
                    ImGui::PushID(directory.c_str());
                    const auto name = std::filesystem::path(directory).filename().string();
                    const ImVec2 tile{std::max(40.f, ImGui::GetContentRegionAvail().x),
                                      (browser.tileSize * .66f + 47.f) * scale};
                    if (EditorDesign::Row(browser.grid ? "##folderTile" : name.c_str(), false,
                                          browser.grid ? tile : ImVec2{}, true, 0, !browser.grid)) {
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            ChangeDirectory(browser, state, directory);
                    }
                    if (browser.grid)
                        DrawAssetCard(name, "Folder", EditorVisuals::Icon::Folder, ImGui::GetItemRectMin(),
                                      ImGui::GetItemRectMax(), false);
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
            if (records.empty() && (searching || directories.size() == 1)) {
                ImGui::TableNextColumn();
                ImGui::TextWrapped(
                    "No matching assets. Import files, select another folder or clear filters.");
            }
            for (const auto *item : records) {
                const auto &record = *item;
                ImGui::TableNextColumn();
                ImGui::PushID(record.id.c_str());
                const auto name = std::filesystem::path(record.relativePath).filename().string();
                const bool selected = state.browserSelection == record.id;
                const ImVec2 tile{std::max(40.f, ImGui::GetContentRegionAvail().x),
                                  (browser.tileSize * .66f + 47.f) * scale};
                if (EditorDesign::Row(browser.grid ? "##assetTile" : name.c_str(), selected,
                                      browser.grid ? tile : ImVec2{}, true, 0, !browser.grid)) {
                    SelectAsset(record, state, db);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        OpenAsset(record, state);
                }
                if (browser.grid && ImGui::IsItemVisible())
                    DrawAssetCard(name, AssetTypeName(record.type), AssetIcon(record.type),
                                  ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), selected, record.missing,
                                  browser.thumbnails->Resolve(record));
                DrawAssetInteraction(record, doc, state, requests, db);
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
    browser.thumbnails->End();
}
