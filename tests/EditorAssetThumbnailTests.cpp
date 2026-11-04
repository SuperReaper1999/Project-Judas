#include "EngineHost.h"
#include "Project.h"
#include "SpriteAnimation.h"
#include "editor/AssetBrowserPanel.h"
#include "editor/EditorDocument.h"
#include "editor/EditorPanels.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool okay, const char *message) {
    ++checks;
    failures += !okay;
    std::printf("%s %s\n", okay ? "PASS" : "FAIL", message);
    std::fflush(stdout);
}
void Bitmap(const std::filesystem::path &path) {
    // An original non-square RGB fixture. BMP rows have exactly 48 bytes.
    std::vector<unsigned char> bytes(54 + 16 * 8 * 3);
    const auto word = [&](unsigned at, unsigned value, unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            bytes[at + i] = (value >> (8 * i)) & 255;
    };
    bytes[0] = 'B';
    bytes[1] = 'M';
    word(2, unsigned(bytes.size()), 4);
    word(10, 54, 4);
    word(14, 40, 4);
    word(18, 16, 4);
    word(22, 8, 4);
    word(26, 1, 2);
    word(28, 24, 2);
    word(34, 16 * 8 * 3, 4);
    for (unsigned i = 54; i < bytes.size(); i += 3) {
        bytes[i] = 40;
        bytes[i + 1] = 90;
        bytes[i + 2] = 220;
    }
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}
} // namespace

int main() try {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
                      ("judas-asset-thumbnails-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{root};
    Project project;
    std::string error;
    if (!Project::CreateNew(root.string(), "Thumbnail controls", project, error))
        throw std::runtime_error("Cannot create isolated thumbnail project: " + error);
    Check(true, "ordinary project fixture is created");
    EngineHost host;
    if (!host.Init("Asset thumbnail qualification", 800, 600, false, error))
        throw std::runtime_error("native graphics unavailable: " + error);
    host.OpenProjectAssets(project.RootDir(), project.AssetsDir());
    auto &assets = host.Assets();
    auto &resources = host.Resources();
    auto &renderer = host.GetRenderer();
    std::vector<AssetRecord> records;
    for (unsigned i = 0; i < 24; ++i) {
        const auto path = fs::path(project.AssetsDir()) / ("image-" + std::to_string(i) + ".bmp");
        Bitmap(path);
        AssetRecord record;
        if (!assets.Track(path.string(), record, error))
            throw std::runtime_error(error);
        records.push_back(record);
    }
    resources.SetBlockingMode(false);
    EditorAssetThumbnailDemand demand;
    demand.Begin(&resources);
    for (const auto &record : records)
        demand.Resolve(record);
    Check(demand.RequestsStarted() == 2 && demand.ActiveCount() == 2,
          "first visible page starts two coalesced asynchronous image requests, not a directory burst");
    for (const auto &record : records)
        demand.Resolve(record);
    Check(demand.RequestsStarted() == 2 && demand.ActiveCount() == 2,
          "repeated same-frame sampling does not duplicate demand or exceed its admission limit");
    demand.End();
    for (unsigned frame = 0; frame < 4; ++frame) {
        // Workers may finish CPU preparation, but no GL-thread Pump publishes
        // them. New UI frames must not accumulate more unknown-size decodes.
        demand.Begin(&resources);
        for (const auto &record : records)
            demand.Resolve(record);
        demand.End();
        Check(demand.RequestsStarted() == 0 && demand.PendingCount() == 2 && demand.ActiveCount() == 2,
              "late workers cannot accumulate additional thumbnail requests across UI frames");
    }
    resources.WaitForAll();
    auto thumbnail = demand.Resolve(records[0]);
    Check(bool(thumbnail) && thumbnail.width == 16 && thumbnail.height == 8,
          "real uploaded image publishes its actual non-square dimensions and native texture token");
    auto valid = resources.TryGetTexture(records[0].id);
    auto stale = valid;
    ++stale.generation;
    Check(renderer.TextureSize(stale) == glm::ivec2(0) && renderer.TextureSize({}) == glm::ivec2(0),
          "stale and absent texture handles have no usable thumbnail dimensions");
    for (unsigned frame = 0; frame < 12; ++frame) {
        demand.Begin(&resources);
        for (const auto &record : records)
            demand.Resolve(record);
        Check(demand.RequestsStarted() <= 2 && demand.ActiveCount() <= 16,
              "successive visible-page frames retain bounded asynchronous work and references");
        demand.End();
        resources.WaitForAll();
    }
    Check(demand.ActiveCount() == 16 && resources.StateOf(records[23].id) == ResourceState::Unloaded,
          "off-page images remain unloaded after the browser's active-demand ceiling");
    demand.Begin(&resources);
    demand.Resolve(records[0]);
    demand.End();
    Check(demand.ActiveCount() == 1 && resources.RefCount(records[1].id) == 0,
          "changing the visible page releases textures that left the page");
    demand.Reset();
    Check(resources.RefCount(records[0].id) == 0 && demand.ActiveCount() == 0,
          "explicit project/shutdown retirement releases all owned browser demand");
    resources.SetBlockingMode(true);
    demand.Begin(&resources);
    Check(!demand.Resolve(records[23]) && demand.RequestsStarted() == 0 &&
              resources.RefCount(records[23].id) == 0,
          "blocking tools never decode unused thumbnails synchronously from the draw path");
    demand.Reset();
    resources.SetBlockingMode(false);
    auto missing = records[23];
    missing.missing = true;
    auto script = records[23];
    script.type = AssetType::Script;
    demand.Begin(&resources);
    Check(!demand.Resolve(missing) && !demand.Resolve(script) && demand.ActiveCount() == 0,
          "missing and non-image records do not start hidden resource work");
    demand.Reset();

    SpriteSheet sheet;
    sheet.texture = records[0].id;
    sheet.width = 16;
    sheet.height = 8;
    sheet.frames.push_back({"whole", "Whole image", {0, 0, 16, 8}, {.5f, .5f}});
    const auto sheetPath = fs::path(project.AssetsDir()) / "image-sheet.judassprite";
    AssetRecord sheetRecord;
    Check(SaveSpriteSheet(sheetPath.string(), sheet, error) &&
              assets.Track(sheetPath.string(), sheetRecord, error),
          "sprite-sheet preview fixture uses a genuine registered sheet and texture dependency");
    demand.Begin(&resources);
    demand.Resolve(sheetRecord);
    demand.End();
    resources.WaitForAll();
    demand.Begin(&resources);
    const auto sheetThumbnail = demand.Resolve(sheetRecord);
    demand.End();
    Check(bool(sheetThumbnail) && sheetThumbnail.image == thumbnail.image && sheetThumbnail.width == 16 &&
              sheetThumbnail.height == 8,
          "sheet preview reuses its actual source texture instead of allocating decorative artwork");
    demand.Reset();
    Check(resources.RefCount(sheetRecord.id) == 0 && resources.RefCount(records[0].id) == 0,
          "sheet retirement releases both metadata and image demand");
    const auto normalBudget = resources.BudgetBytes();
    resources.SetBudgetBytes(512);
    demand.Begin(&resources);
    const auto oversized = demand.Resolve(records[0]);
    demand.End();
    Check(bool(oversized) && demand.ActiveCount() == 0 && resources.RefCount(records[0].id) == 0,
          "already-resident images beyond the thumbnail pin budget remain unpinned borrowed views");
    demand.Reset();
    resources.SetBudgetBytes(normalBudget);

    // Draw the actual asset browser, not a substitute image widget. Its image
    // command must carry a 2:1 quad, not stretch the source into a square card.
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {900, 700};
    io.DeltaTime = 1.f / 60;
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    EditorDocument document;
    EditorPanelState state;
    EditorRequests requests;
    state.project = &project;
    state.assets = &assets;
    state.resources = &resources;
    state.assetSearch = records[0].id;
    const auto texturesBefore = resources.Stats().loadedTextures;
    for (unsigned frame = 0; frame < 3; ++frame) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({850, 650});
        ImGui::Begin("Actual asset browser controls");
        DrawAssetList(document, state, requests, assets);
        ImGui::End();
        ImGui::Render();
    }
    bool imageCommand = false, correctAspect = false;
    const auto token = renderer.EditorImageToken(resources.TryGetTexture(records[0].id));
    const auto *drawData = ImGui::GetDrawData();
    for (int list = 0; list < drawData->CmdListsCount; ++list) {
        const auto *draw = drawData->CmdLists[list];
        for (const auto &command : draw->CmdBuffer) {
            if (command.GetTexID() != ImTextureID(token) || command.ElemCount != 6)
                continue;
            imageCommand = true;
            ImVec2 lo{INFINITY, INFINITY}, hi{-INFINITY, -INFINITY};
            for (unsigned index = command.IdxOffset; index < command.IdxOffset + command.ElemCount; ++index) {
                const auto point = draw->VtxBuffer[draw->IdxBuffer[index] + command.VtxOffset].pos;
                lo.x = std::min(lo.x, point.x);
                lo.y = std::min(lo.y, point.y);
                hi.x = std::max(hi.x, point.x);
                hi.y = std::max(hi.y, point.y);
            }
            correctAspect |= hi.y > lo.y && std::abs((hi.x - lo.x) / (hi.y - lo.y) - 2.f) < .001f;
        }
    }
    Check(imageCommand && correctAspect,
          "actual browser draws the resident image with its original 2:1 aspect ratio");
    Check(resources.Stats().loadedTextures == texturesBefore,
          "browser image presentation reuses resource-manager textures without duplicate GPU uploads");
    Check(state.assetBrowser && state.assetBrowser->thumbnails->ActiveCount() == 1,
          "actual visible browser owns only the sampled image demand");
    state.showAssetBrowser = false;
    PumpEditorAssetThumbnails(state);
    Check(state.assetBrowser->thumbnails->ActiveCount() == 0 && resources.RefCount(records[0].id) == 0,
          "hiding the browser releases its outstanding image demand");
    state.showAssetBrowser = true;
    state.mode = EditorMode::Play;
    state.assetBrowser->thumbnails->Begin(&resources);
    state.assetBrowser->thumbnails->Resolve(records[0]);
    PumpEditorAssetThumbnails(state);
    Check(state.assetBrowser->thumbnails->ActiveCount() == 0,
          "entering Play retires editor-only thumbnail demand");
    state.mode = EditorMode::Edit;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({850, 650});
    ImGui::Begin("Actual asset browser controls");
    DrawAssetList(document, state, requests, assets);
    ImGui::End();
    const auto countCommands = [&](ImTextureID id) {
        unsigned count = 0;
        for (const auto *window : ImGui::GetCurrentContext()->Windows)
            if (window->LastFrameActive == ImGui::GetFrameCount())
                for (const auto &command : window->DrawList->CmdBuffer)
                    count += command.GetTexID() == id;
        return count;
    };
    const auto oldImage = ImTextureID(renderer.EditorImageToken(resources.TryGetTexture(records[0].id)));
    const auto fallback = ImTextureID(renderer.EditorImageToken({}));
    const auto beforeRetirement = countCommands(oldImage);
    const auto textBefore = countCommands(io.Fonts->TexID);
    ResetEditorAssetThumbnails(state);
    resources.Release(records[0].id); // Same destructive boundary as an asset rescan/project switch.
    Check(beforeRetirement > 0 && oldImage != fallback && countCommands(oldImage) == 0 &&
              countCommands(fallback) >= beforeRetirement && countCommands(io.Fonts->TexID) == textBefore,
          "active-frame retirement replaces sampled image commands before deletion and leaves text intact");
    Check(renderer.TextureSize(valid) == glm::ivec2(0),
          "retirement negative control genuinely deleted the original GPU image before submission");
    ImGui::Render();
    ResetEditorAssetThumbnails(state);
    ImGui::DestroyContext();
    std::printf("Asset thumbnails: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Asset thumbnail fixture failed: %s\n", error.what());
    return 1;
}
