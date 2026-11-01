#include "editor/UtilityAIEditor.h"
#include "editor/EditorPanels.h"
#include "UtilityAI.h"
#include "Project.h"
#include "imgui.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using Json = nlohmann::json;
std::string Read(const std::string& path) {
    std::ifstream in(fs::u8path(path), std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
int main() {
    unsigned checks = 0;
    const auto check = [&](bool okay, const std::string& label) {
        ++checks;
        if (!okay) throw std::runtime_error(label);
        std::cout << "PASS " << label << '\n';
    };
    const auto root = fs::temp_directory_path() / ("judas-ai-editor-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::string output = "unchanged", error;
        check(!DecodeUtilityAIModule("export default (() => { throw Error('must never run'); })();", output, error)
              && output == "unchanged", "editor never evaluates code or publishes a failed parse");
        check(!DecodeUtilityAIModule("export default {\"version\":1,\"actions\":[{\"id\":\"idle\",\"weight\":1,\"weight\":2}]};", output, error)
              && output == "unchanged", "duplicate definition keys survive to the native rejection boundary");
        check(!DecodeUtilityAIModule("export default {\"version\":1,\"actions\":[]}; console.log('x');", output, error),
              "extra source code is rejected");
        check(!DecodeUtilityAIModule("export default {\"version\":1,\"actions\":[{\"id\":\"idle\"}],\"typo\":1};", output, error),
              "runtime and editor share strict unknown-field validation");
        const auto module = "export default {\"version\":1,\"actions\":[{\"id\":\"idle\"}]};\n";
        check(DecodeUtilityAIModule(module, output, error), "valid minimal definition receives native defaults");
        check(Json::parse(output).at("actions")[0].contains("considerations"), "defaults are authorable data");
        const auto encoded = EncodeUtilityAIModule(output);
        std::string decoded;
        check(DecodeUtilityAIModule(encoded, decoded, error) && decoded == output, "module encoding is a canonical semantic roundtrip");
        Project project;
        check(Project::CreateNew(root.string(), "AI authoring", project, error), "create isolated ordinary project");
        AssetDatabase db; db.Scan(project.RootDir(), project.AssetsDir());
        UtilityAIEditor editor;
        check(editor.Create(db, "Citizen.ai.js", error), "create and register ordinary script module: " + error);
        check(!editor.asset.empty() && db.Find(editor.asset)->type == AssetType::Script && !editor.Dirty(),
              "AI definition participates in existing script asset identity");
        const auto originalPath = editor.path, originalId = editor.asset;
        const auto originalSource = Read(editor.path);
        check(!editor.Create(db, "Citizen.ai.js", error) && Read(originalPath) == originalSource,
              "creation cannot overwrite an existing asset");
        check(!editor.Create(db, "../Escape.js", error) && !fs::exists(root / "Escape.js"), "new names cannot escape the project");
        auto draft = Json::parse(editor.definition); draft["name"] = "Changed citizen";
        editor.Remember(); editor.definition = draft.dump();
        check(editor.Dirty(), "editing marks a draft dirty");
        EditorPanelState guarded;
        check(!editor.Guard(guarded) && guarded.showUtilityAI && !guarded.status.empty(),
              "unsaved draft blocks leaving, Play and export with actionable panel feedback");
        check(!editor.Open(*db.Find(originalId), error) && editor.Dirty(), "switching cannot discard an unsaved draft");
        check(editor.Save(db, error) && !editor.Dirty(), "explicit save persists and clears draft dirty state");
        UtilityAIEditor reopened;
        check(reopened.Open(*db.Find(originalId), error) && Json::parse(reopened.definition).at("name") == "Changed citizen",
              "new editor session restores the authored definition");
        const auto saved = Read(editor.path);
        editor.definition = "{\"version\":1,\"actions\":[]}";
        check(!editor.Save(db, error) && Read(editor.path) == saved && editor.Dirty(),
              "invalid scoring configuration cannot replace valid disk content");
        editor.definition = reopened.definition;
        { std::ofstream out(editor.path, std::ios::binary); out << saved << "\n"; }
        draft = Json::parse(editor.definition); draft["name"] = "Conflicting draft"; editor.definition = draft.dump();
        const auto externalBytes = Read(editor.path);
        check(!editor.Save(db, error) && Read(editor.path) == externalBytes && editor.Dirty(),
              "external edit conflict preserves both disk bytes and unsaved draft");
        editor.Discard();
        check(editor.Open(*db.Find(originalId), error), "explicit reopen recovers after conflict");
        auto arbitrary = *db.Find(originalId);
        arbitrary.path = (root / "ordinary.js").string();
        { std::ofstream out(arbitrary.path); out << "export default class { start() {} }"; }
        const auto retained = editor.definition;
        check(!editor.Open(arbitrary, error) && editor.definition == retained && editor.path == originalPath,
              "ordinary behaviour scripts are rejected without destroying the draft");
        AssetDatabase empty;
        check(!editor.Save(empty, error) && Read(originalPath) == externalBytes,
              "foreign asset database cannot save a previous project's draft");
#ifndef _WIN32
        const auto alias = root.parent_path() / (root.filename().string() + "-alias");
        fs::create_directory_symlink(root, alias);
        Project aliased;
        check(aliased.Load((alias / fs::path(project.ProjectFile()).filename()).string(), error), "open ordinary project through a directory alias");
        AssetDatabase aliasDB; aliasDB.Scan(aliased.RootDir(), aliased.AssetsDir());
        UtilityAIEditor aliasEditor;
        check(aliasEditor.Create(aliasDB, "Aliased.ai.js", error) && fs::is_regular_file(aliasEditor.path),
              "definition creation preserves legitimate aliased project ownership");
        fs::remove(alias);
        const auto escaped = root / "Foreign";
        fs::create_directories(escaped);
        const auto nested = root / "Other";
        Project other;
        check(Project::CreateNew(nested.string(), "Other", other, error), "create symlink ownership control");
        fs::create_directory_symlink(escaped, fs::path(other.AssetsDir()) / "AI");
        AssetDatabase otherDB; otherDB.Scan(other.RootDir(), other.AssetsDir());
        UtilityAIEditor blocked;
        check(!blocked.Create(otherDB, "Escape.ai.js", error) && !fs::exists(escaped / "Escape.ai.js"),
              "symlinked AI folder cannot publish outside the asset owner");
#endif
        Project unicodeProject;
        const auto unicodeRoot = root / fs::u8path(u8"Créatures");
        check(Project::CreateNew(unicodeRoot.u8string(), "Unicode AI", unicodeProject, error), "create project at a Unicode directory");
        check(fs::is_directory(unicodeRoot) && fs::equivalent(fs::u8path(unicodeProject.RootDir()), unicodeRoot),
              "project creation uses the actual Unicode directory without a code-page alias");
        AssetDatabase unicodeDB; unicodeDB.Scan(unicodeProject.RootDir(), unicodeProject.AssetsDir());
        UtilityAIEditor unicodeEditor;
        const bool unicodeCreated = unicodeEditor.Create(unicodeDB, u8"Citoyen-é.ai.js", error);
        check(unicodeCreated, "create definition using a Unicode staging/output path: " + error);
        const auto unicodeDefinition = unicodeRoot / "Assets" / "AI" / fs::u8path(u8"Citoyen-é.ai.js");
        const auto unicodeMeta = fs::u8path(unicodeDefinition.u8string() + ".judasmeta");
        check(fs::is_regular_file(unicodeDefinition) && fs::is_regular_file(unicodeMeta) &&
              fs::equivalent(fs::u8path(unicodeEditor.path), unicodeDefinition),
              "source and metadata are published at their real Unicode paths");
        const auto unicodeSource = Read(unicodeEditor.path), unicodeMetadata = Read(unicodeMeta.u8string());
        check(!unicodeEditor.Create(unicodeDB, u8"Citoyen-é.ai.js", error) &&
              Read(unicodeEditor.path) == unicodeSource && Read(unicodeMeta.u8string()) == unicodeMetadata,
              "duplicate Unicode creation preserves authored source and metadata");
        const auto reservedDefinition = unicodeDefinition.parent_path() / fs::u8path(u8"Réservé.ai.js");
        const auto reservedMeta = fs::u8path(reservedDefinition.u8string() + ".judasmeta");
        { std::ofstream file(reservedMeta, std::ios::binary); file << "Preserved metadata ownership"; }
        check(!unicodeEditor.Create(unicodeDB, u8"Réservé.ai.js", error) && !fs::exists(reservedDefinition) &&
              Read(reservedMeta.u8string()) == "Preserved metadata ownership",
              "Unicode metadata alone prevents creation without changing the owned sidecar");
        auto unicodeDraft = Json::parse(unicodeEditor.definition); unicodeDraft["name"] = u8"Citoyen éveillé";
        unicodeEditor.definition = unicodeDraft.dump();
        const bool unicodeSaved = unicodeEditor.Save(unicodeDB, error);
        check(unicodeSaved, "save authored Unicode content through staged publication: " + error);
        UtilityAIEditor unicodeReopened;
        check(unicodeReopened.Open(*unicodeDB.Find(unicodeEditor.asset), error) && unicodeReopened.definition == unicodeEditor.definition,
              "Unicode definition survives a fresh editor model");
        unicodeDraft["name"] = "Unsaved draft"; unicodeEditor.definition = unicodeDraft.dump();
        { std::ofstream file(unicodeDefinition, std::ios::binary); file << unicodeEditor.savedSource << "\n"; }
        const auto unicodeExternal = Read(unicodeEditor.path);
        check(!unicodeEditor.Save(unicodeDB, error) && unicodeEditor.Dirty() &&
              Read(unicodeEditor.path) == unicodeExternal && Read(unicodeMeta.u8string()) == unicodeMetadata,
              "Unicode save conflict preserves external bytes, metadata and unsaved draft");
        bool noUnicodeStaging = true;
        for (const auto& file : fs::directory_iterator(unicodeDefinition.parent_path()))
            noUnicodeStaging &= file.path().filename().u8string().find(".import-staging-") == std::string::npos;
        check(noUnicodeStaging, "Unicode create/save leaves no owned staging files behind");
        ImGui::CreateContext();
        auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = {1400, 900}; io.DeltaTime = 1.f / 60;
        unsigned char* pixels = nullptr; int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        EditorPanelState state; state.project = &project; state.assets = &db;
        state.showUtilityAI = true; state.utilityAIEdit = originalId;
        for (bool light : {false, true}) for (float scale : {.85f, 1.f, 1.5f}) {
            ImGui::GetStyle() = ImGuiStyle{};
            if (light) ImGui::StyleColorsLight(); else ImGui::StyleColorsDark();
            ImGui::GetStyle().ScaleAllSizes(scale); io.FontGlobalScale = scale;
            ImGui::NewFrame(); editor.Panel(state); ImGui::Render();
            check(ImGui::GetDrawData() && ImGui::GetDrawData()->TotalVtxCount > 0,
                  std::string(light ? "light" : "dark") + " authoring panel renders at scale " + std::to_string(scale));
        }
        ImGui::DestroyContext();
        check(Read(originalPath) == externalBytes, "theme/scale render checks never save source implicitly");
        fs::remove_all(root);
        std::cout << "Utility AI editor: " << checks << " checks, 0 failures\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        std::cerr << "Fixture preserved: " << root << '\n';
        if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
        return 1;
    }
}
