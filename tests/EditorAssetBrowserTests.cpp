#include "AssetCook.h"
#include "AssetDatabase.h"
#include "Project.h"
#include "ModelCook.h"
#include "ModelImportScale.h"
#include "editor/AssetBrowserModel.h"
#include "editor/AssetBrowserPanel.h"
#include "editor/AssetCookPanelTask.h"
#include "editor/AssetPublicationWorkflow.h"
#include "editor/EditorPanels.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char *label) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", label);
}
} // namespace
int main() {
    namespace fs = std::filesystem;
    const auto root =
        fs::temp_directory_path() /
        ("judas-browser-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "Assets/scripts/npc");
    fs::create_directories(root / "Assets/scripts-old");
    AssetDatabase db;
    db.Scan(root.string(), (root / "Assets").string());
    AssetRecord player, npc, sibling;
    std::string error;
    for (const auto &path :
         {"Assets/scripts/player.js", "Assets/scripts/npc/guard.js", "Assets/scripts-old/old.js"})
        std::ofstream(root / path) << "export default class {}\n";
    Check(db.Track((root / "Assets/scripts/player.js").string(), player, error) &&
              db.Track((root / "Assets/scripts/npc/guard.js").string(), npc, error) &&
              db.Track((root / "Assets/scripts-old/old.js").string(), sibling, error),
          "ordinary asset database tracks real project scripts");
    Check(EditorAssetBrowserPath(player, db.AssetsDir()) == "scripts/player.js",
          "browser paths are relative to Assets, retaining database IDs");
    const auto folders = EditorAssetBrowserDirectories(db);
    Check(folders == std::vector<std::string>{"", "scripts", "scripts-old", "scripts/npc"},
          "folder navigation includes all registered parent folders in stable order");
    auto files = EditorAssetBrowserRecords(db, "scripts", false);
    Check(files.size() == 1 && files.front()->id == player.id,
          "opening a folder shows its immediate files");
    files = EditorAssetBrowserRecords(db, "scripts", true);
    Check(files.size() == 2 && files[0]->id == npc.id && files[1]->id == player.id,
          "recursive browsing includes descendants and stable filename sorting");
    Check(!EditorAssetBrowserContains("scripts-old/old.js", "scripts", true),
          "directory matching cannot include similarly prefixed siblings");
    Check(EditorAssetBrowserRecords(db, "scripts", true, int(AssetType::Texture)).empty(),
          "type filter is applied to actual records");
    Check(EditorAssetBrowserRecords(db, "scripts", true, -1, true).empty(),
          "missing-only filter excludes available assets");
    fs::remove(root / "Assets/scripts/player.js");
    db.Scan(root.string(), (root / "Assets").string());
    files = EditorAssetBrowserRecords(db, "scripts", false, -1, true);
    Check(files.size() == 1 && files.front()->id == player.id && files.front()->missing,
          "missing sidecar-backed assets remain discoverable in their folder");
    auto external = player;
    external.path = (root / "unrelated.js").string();
    Check(EditorAssetBrowserPath(external, db.AssetsDir()).find("External/") == 0,
          "out-of-tree records are inspectable without navigation escape");
    auto mod = npc;
    mod.owner = "example/mod";
    mod.readOnly = true;
    Check(EditorAssetBrowserPath(mod, db.AssetsDir()) == "Mods/example_mod/Assets/scripts/npc/guard.js",
          "mod ownership is visibly separate from writable project folders");
    EditorPanelState first, second;
    first.assetBrowser = std::make_shared<EditorAssetBrowserState>();
    second.assetBrowser = std::make_shared<EditorAssetBrowserState>();
    Check(first.assetBrowser->grid && second.assetBrowser->grid,
          "fresh editor sessions start with the documented asset grid view");
    first.assetBrowser->directory = "scripts";
    first.assetBrowser->grid = false;
    Check(second.assetBrowser->directory.empty() && second.assetBrowser->grid &&
              first.assetBrowser->directory == "scripts" && !first.assetBrowser->grid,
          "browser folder/view state belongs to its editor session");
    Project project;
    Check(Project::CreateNew(root.string(), "Browser publication", project, error),
          "publication fixture is an ordinary saved project");
    const auto source = root / "Assets/triangle.obj";
    std::ofstream(source) << "o triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    auto prepared = std::make_shared<AssetCookPanelTask>();
    prepared->root = project.RootDir();
    prepared->source = source.string();
    prepared->destination = (root / "Assets/triangle.packed.judasmesh").string();
    AssetCookSettings settings;
    Check(PrepareAssetCook(prepared->source, prepared->destination, settings, prepared->bytes,
                           prepared->recipe, prepared->error),
          "real asset cook prepares valid bytes before authoring publication");
    prepared->ready.store(true);
    EditorPanelState publishing;
    publishing.project = &project;
    publishing.assets = &db;
    publishing.showAssetBrowser = false;
    publishing.assetCookTask = prepared;
    EditorRequests requests;
    PumpEditorAssetWorkflow(publishing, requests);
    Check(fs::is_regular_file(prepared->destination) &&
              requests.trackAssetPath == prepared->destination && !publishing.assetCookTask,
          "hidden browser still publishes a prepared generation through its independent workflow");
    auto blocked = std::make_shared<AssetCookPanelTask>();
    blocked->root = prepared->root;
    blocked->bytes = prepared->bytes;
    blocked->recipe = prepared->recipe;
    blocked->destination = (root / "Assets/blocked.judasmesh").string();
    blocked->ready.store(true);
    publishing.mode = EditorMode::Play;
    publishing.assetCookTask = blocked;
    requests = {};
    PumpEditorAssetWorkflow(publishing, requests);
    Check(!fs::exists(blocked->destination) && publishing.assetCookTask == blocked &&
              requests.trackAssetPath.empty(),
          "Play-time publication is refused even if the browser and worker are ready");
    publishing.mode = EditorMode::Edit;
    blocked->root = (root / "previous-project").string();
    PumpEditorAssetWorkflow(publishing, requests);
    Check(!fs::exists(blocked->destination) && !publishing.assetCookTask &&
              requests.trackAssetPath.empty(),
          "previous-project completion cannot publish into the current authoring project");
    // Real cooked model publication pauses on scale advice, even while the
    // browser is hidden, and retains the accepted generation until confirmed.
    const auto largeSource = root / "large.obj";
    std::ofstream(largeSource) << "o wall\nv 0 0 0\nv 200 0 0\nv 0 200 0\nf 1 2 3\n";
    std::string scaleRecipe;
    Check(CreateModelRecipe(root.string(), largeSource.string(), "Assets/large.judasmodel", scaleRecipe, error),
          "scale-review fixture creates a real source-owned import recipe");
    auto large = std::make_shared<ModelCookTask>();
    Check(CookModelRecipe(scaleRecipe, *large) && JudasImport::ScaleWarning(large->report),
          "real model cook exposes suspicious normalized dimensions before publication");
    large->done = true;
    publishing.mode = EditorMode::Edit;
    publishing.importTask = large;
    publishing.importPublished = false;
    publishing.importScaleConfirmed.reset();
    publishing.importScalePrompted.reset();
    const auto lastGood = std::make_shared<ModelCookTask>();
    publishing.importAccepted = lastGood;
    publishing.showAssetBrowser = false;
    requests = {};
    PumpEditorAssetWorkflow(publishing, requests);
    Check(EditorModelScaleReviewPending(publishing) && !fs::exists(large->output) &&
              publishing.importAccepted == lastGood && !requests.rescanAssets &&
              publishing.showAssetBrowser && publishing.selectModelImportTools,
          "100x review reveals its controls and cannot silently replace the last accepted asset");
    publishing.importScaleConfirmed = large;
    PumpEditorAssetWorkflow(publishing, requests);
    Check(publishing.importPublished && publishing.importAccepted == large &&
              fs::is_regular_file(large->output) && requests.rescanAssets,
          "explicit keep-scale confirmation publishes the intact legitimate large model");
    auto repeated = std::make_shared<ModelCookTask>();
    Check(CookModelRecipe(scaleRecipe, *repeated) && repeated->unchanged &&
              JudasImport::ScaleWarning(repeated->report),
          "unchanged receipt retains scale advice for a later editor session");
    repeated->done = true;
    publishing.importTask = repeated;
    publishing.importPublished = false;
    Check(EditorModelScaleReviewPending(publishing),
          "approval belongs to the exact cooked task and cannot leak to a later direct reimport");
    requests = {};
    publishing.mode = EditorMode::Play;
    PumpEditorAssetWorkflow(publishing, requests);
    Check(!publishing.importPublished && !requests.rescanAssets,
          "scale confirmation cannot bypass the existing Edit-only publication boundary");
    publishing.mode = EditorMode::Edit;
    repeated->cancel = true;
    publishing.importPublished = true; // same deliberate cancel action as the card
    PumpEditorAssetWorkflow(publishing, requests);
    Check(publishing.importAccepted == large && !requests.rescanAssets,
          "cancelling scale review leaves the last accepted model and asset demand intact");
    fs::remove_all(root);
    std::printf("SUMMARY %d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
