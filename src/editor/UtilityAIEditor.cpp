#include "UtilityAIEditor.h"
#include "EditorPanels.h"
#include "PlatformServices.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "UtilityAI.h"
#include "imgui.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
constexpr std::size_t maximumSource = 1024 * 1024;

std::string ReadSource(const fs::path& path) {
    if (fs::file_size(path) > maximumSource) throw std::runtime_error("AI definition exceeds the 1 MiB editor limit.");
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open AI definition.");
    return {std::istreambuf_iterator<char>(in), {}};
}
bool Text(const char* label, std::string& value) {
    std::vector<char> buffer(std::max<std::size_t>(value.size() + 256, 1024), 0);
    std::copy(value.begin(), value.end(), buffer.begin());
    if (!ImGui::InputText(label, buffer.data(), buffer.size())) return false;
    value = buffer.data();
    return true;
}
bool JsonText(const char* label, Json& object, const char* key) {
    auto value = object.at(key).get<std::string>();
    if (!Text(label, value)) return false;
    object[key] = value;
    return true;
}
bool Number(const char* label, Json& object, const char* key) {
    double value = object.at(key).get<double>();
    if (!ImGui::InputDouble(label, &value, .05, .5, "%.4g")) return false;
    if (!std::isfinite(value)) return false;
    object[key] = value;
    return true;
}
void OwnPath(const AssetDatabase& db, const fs::path& path) {
    const auto root = fs::canonical(fs::u8path(db.AssetsDir()));
    const auto resolved = fs::weakly_canonical(path);
    const auto relative = resolved.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() ||
        std::any_of(relative.begin(), relative.end(), [](const auto& p) { return p == ".."; }))
        throw std::runtime_error("AI definition must belong to this project's assets folder.");
}
Json DefaultAction(const std::string& id) {
    return {{"id", id}, {"weight", 1}, {"mode", "product"}, {"cooldown", 0},
            {"minCommitment", 0}, {"interruptible", true}, {"requiredFacts", Json::array()},
            {"considerations", Json::array()}};
}
Json DefaultConsideration() {
    return {{"fact", "need"}, {"weight", 1}, {"min", 0}, {"max", 1}, {"curve", "linear"}, {"exponent", 2}};
}
std::string DefaultDefinition() {
    return UtilityAgent::NormalizeDefinition(Json{{"version", 1}, {"name", "Citizen"},
        {"interval", .2}, {"switchAdvantage", .1}, {"actions", {DefaultAction("idle")}}}.dump());
}
void JsonTree(const Json& value, int depth = 0) {
    if (depth > 6) { ImGui::TextDisabled("Further nested detail omitted"); return; }
    if (!value.is_structured()) { ImGui::TextWrapped("%s", value.dump().c_str()); return; }
    std::size_t shown = 0;
    for (auto i = value.begin(); i != value.end(); ++i) {
        if (shown++ == 128) { ImGui::TextDisabled("Additional rows omitted"); break; }
        const auto key = value.is_array() ? std::to_string(shown - 1) : i.key();
        ImGui::PushID(static_cast<int>(shown));
        if (i->is_structured()) {
            if (ImGui::TreeNode(key.c_str())) { JsonTree(*i, depth + 1); ImGui::TreePop(); }
        } else ImGui::TextWrapped("%s: %s", key.c_str(), i->dump().c_str());
        ImGui::PopID();
    }
}
} // namespace

bool DecodeUtilityAIModule(const std::string& source, std::string& definition, std::string& error) {
    try {
        if (source.size() > maximumSource) throw std::runtime_error("AI module exceeds the 1 MiB editor limit.");
        const auto first = source.find_first_not_of(" \t\r\n");
        constexpr const char* prefix = "export default";
        if (first == std::string::npos || source.compare(first, 14, prefix) != 0)
            throw std::runtime_error("Not an AI definition module: expected export default followed by JSON.");
        auto json = source.substr(first + 14);
        const auto end = json.find_last_not_of(" \t\r\n");
        if (end != std::string::npos && json[end] == ';') json.erase(end);
        // No comments, code, imports or expression evaluation: parse JSON only.
        const auto normalized = UtilityAgent::NormalizeDefinition(json);
        definition = normalized;
        error.clear();
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
std::string EncodeUtilityAIModule(const std::string& definition) {
    return "export default " + Json::parse(UtilityAgent::NormalizeDefinition(definition)).dump(2) + ";\n";
}
bool UtilityAIEditor::Dirty() const { return !path.empty() && definition != savedDefinition; }
bool UtilityAIEditor::Guard(EditorPanelState& state) const {
    if (!Dirty()) return true;
    state.showUtilityAI = true;
    state.status = "Save or discard the AI definition draft before leaving, playing or exporting";
    return false;
}
void UtilityAIEditor::Remember() {
    if (undo.size() >= 64) undo.erase(undo.begin());
    undo.push_back(definition);
}
void UtilityAIEditor::Discard() {
    asset.clear(); path.clear(); definition.clear(); savedDefinition.clear(); savedSource.clear();
    undo.clear(); action = 0;
}
bool UtilityAIEditor::Open(const AssetRecord& record, std::string& error) {
    try {
        if (Dirty()) throw std::runtime_error("Save or discard the current AI draft before switching assets.");
        if (record.type != AssetType::Script || record.missing)
            throw std::runtime_error("Choose an existing AI definition script module.");
        const auto source = ReadSource(fs::u8path(record.path));
        std::string candidate;
        if (!DecodeUtilityAIModule(source, candidate, error)) return false;
        asset = record.id; path = record.path; definition = savedDefinition = std::move(candidate);
        savedSource = source; undo.clear(); action = 0; error.clear();
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool UtilityAIEditor::Create(AssetDatabase& db, const std::string& fileName, std::string& error) {
    std::string staged;
    try {
        if (Dirty()) throw std::runtime_error("Save or discard the current AI draft first.");
        auto name = fs::u8path(fileName);
        if (fileName.empty() || fileName.size() > 128 || fileName.find_first_of("/\\:") != std::string::npos ||
            name != name.filename() || name.stem().empty() || name == "." || name == "..")
            throw std::runtime_error("Enter a file name such as Citizen.ai.js, without folders.");
        if (name.extension().empty()) name += ".js";
        if (name.extension() != ".js") throw std::runtime_error("AI definition modules must end in .js.");
        const auto directory = fs::u8path(db.AssetsDir()) / "AI";
        OwnPath(db, directory);
        fs::create_directories(directory);
        const auto destination = directory / name;
        OwnPath(db, destination);
        if (fs::symlink_status(destination).type() != fs::file_type::not_found ||
            fs::symlink_status(fs::u8path(destination.u8string() + ".judasmeta")).type() != fs::file_type::not_found)
            throw std::runtime_error("That asset or metadata already exists; open it rather than overwrite it.");
        const auto source = EncodeUtilityAIModule(DefaultDefinition());
        staged = CreateImportStagingFile(destination);
        { std::ofstream out(fs::u8path(staged), std::ios::binary); out << source; out.close();
          if (!out) throw std::runtime_error("Cannot write AI definition staging file."); }
        fs::copy_file(fs::u8path(staged), destination, fs::copy_options::none);
        fs::remove(fs::u8path(staged)); staged.clear();
        AssetRecord record;
        if (!db.Track(destination.u8string(), record, error)) {
            error = "Source preserved at " + destination.u8string() + "; registration failed: " + error;
            return false;
        }
        return Open(record, error);
    } catch (const std::exception& e) {
        if (!staged.empty()) { std::error_code ignored; fs::remove(fs::u8path(staged), ignored); }
        error = e.what(); return false;
    }
}
bool UtilityAIEditor::Save(AssetDatabase& db, std::string& error) {
    std::string staged;
    try {
        const auto* record = db.Find(asset);
        if (!record || record->path != path || record->type != AssetType::Script || record->readOnly || record->missing)
            throw std::runtime_error("Choose a writable AI definition in this project.");
        OwnPath(db, fs::u8path(path));
        const auto source = EncodeUtilityAIModule(definition);
        if (ReadSource(fs::u8path(path)) != savedSource)
            throw std::runtime_error("AI source changed on disk. Draft retained; reopen before overwriting.");
        staged = CreateImportStagingFile(fs::u8path(path));
        { std::ofstream out(fs::u8path(staged), std::ios::binary); out << source; out.close();
          if (!out) throw std::runtime_error("Cannot write AI definition staging file."); }
        ReplaceStagedFile(fs::u8path(staged), fs::u8path(path)); staged.clear();
        savedSource = source; definition = savedDefinition = UtilityAgent::NormalizeDefinition(definition);
        error.clear(); return true;
    } catch (const std::exception& e) {
        if (!staged.empty()) { std::error_code ignored; fs::remove(fs::u8path(staged), ignored); }
        error = e.what(); return false;
    }
}

std::vector<ScriptAIInspection> EditorAIAgentsForEntity(const std::vector<ScriptAIInspection>& agents,SceneObjectId entity) {
    if(!entity)return agents;
    std::vector<ScriptAIInspection> selected;
    for(const auto& agent:agents)if(agent.entity==entity)selected.push_back(agent);
    return selected;
}

void UtilityAIEditor::Panel(EditorPanelState& state) {
    if (!state.showUtilityAI) return;
    ImGui::SetNextWindowSize({820, 650}, ImGuiCond_FirstUseEver);
    if(state.utilityAILiveRequested||state.utilityAIAuthorRequested)ImGui::SetNextWindowCollapsed(false,ImGuiCond_Always);
    if (!ImGui::Begin("AI actions and decisions", &state.showUtilityAI)) { ImGui::End(); return; }
    const bool requestedAuthor=state.utilityAIAuthorRequested;state.utilityAIAuthorRequested=false;
    const bool requestedLive=state.utilityAILiveRequested&&!requestedAuthor;
    state.utilityAILiveRequested=false;
    if(requestedLive)followSelected=false;
    if (ImGui::BeginTabBar("aiTabs")) {
        if (ImGui::BeginTabItem("Author actions",nullptr,requestedAuthor?ImGuiTabItemFlags_SetSelected:0)) {
            ImGui::TextWrapped("Choose what an NPC values here. Project scripts provide candidates, sensing and action execution.");
            const bool editableProject = state.mode == EditorMode::Edit && state.assets && state.project && state.project->IsLoaded();
            ImGui::BeginDisabled(!editableProject);
            Text("New definition", newName);
            ImGui::SameLine();
            if (ImGui::Button("Create")) {
                if (Create(*state.assets, newName, status)) {
                    state.utilityAIEdit = asset;
                    status = "Created in Assets/AI. Import this module in your NPC script; it is not an attachable behaviour class.";
                }
            }
            ImGui::EndDisabled();
            if (state.assets && ImGui::BeginCombo("Existing definition", path.empty() ? "Choose a script module" : fs::u8path(path).filename().u8string().c_str())) {
                for (const auto& [id, record] : state.assets->Records()) {
                    if (record.type != AssetType::Script || record.missing) continue;
                    if (ImGui::Selectable(record.relativePath.c_str(), id == asset)) state.utilityAIEdit = id;
                }
                ImGui::EndCombo();
            }
            const auto* requested = state.assets ? state.assets->Find(state.utilityAIEdit) : nullptr;
            if (requested && requested->path != path) {
                if (Dirty()) {
                    ImGui::TextWrapped("Unsaved draft retained. Save or discard before opening another definition.");
                } else if (!Open(*requested, status)) {
                    // Failed reads preserve the current draft. Stop retrying until requested again.
                    state.utilityAIEdit = asset;
                }
            }
            if (!path.empty()) {
                const auto* current = state.assets ? state.assets->Find(asset) : nullptr;
                const bool writable = editableProject && current && current->path == path && !current->readOnly && !current->missing;
                ImGui::TextWrapped("%s%s", path.c_str(), Dirty() ? "  (unsaved)" : "");
                ImGui::BeginDisabled(!writable);
                if (ImGui::Button("Save definition")) {
                    if (Save(*state.assets, status)) status = "Saved. Stop/Play or deliberately reload scripts to apply the definition.";
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(undo.empty());
                if (ImGui::Button("Undo draft edit") && !undo.empty()) {
                    definition = std::move(undo.back()); undo.pop_back();
                }
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Discard / close draft")) { Discard(); state.utilityAIEdit.clear(); }
                if (!definition.empty()) {
                    ImGui::BeginDisabled(!writable);
                    auto draft = Json::parse(definition);
                    bool changed = JsonText("Name", draft, "name");
                    changed |= Number("Decision interval (seconds)", draft, "interval");
                    if (ImGui::CollapsingHeader("Evaluation scheduling")) {
                        changed |= Number("Initial phase delay (seconds)", draft, "initialDelay");
                        ImGui::TextWrapped("Choose a delay from zero to the decision interval to spread agents deterministically across ticks.");
                    }
                    changed |= Number("Switch advantage", draft, "switchAdvantage");
                    ImGui::TextDisabled("Simulation time, not rendering frequency. Larger intervals reduce decision work.");
                    auto& actions = draft.at("actions");
                    if (ImGui::BeginCombo("Action", actions[std::clamp(action, 0, int(actions.size()) - 1)].at("id").get<std::string>().c_str())) {
                        for (std::size_t i = 0; i < actions.size(); ++i)
                            if (ImGui::Selectable(actions[i].at("id").get<std::string>().c_str(), action == int(i))) action = int(i);
                        ImGui::EndCombo();
                    }
                    ImGui::BeginDisabled(actions.size() >= 64);
                    if (ImGui::Button("Add action")) {
                        unsigned serial = 1; std::string id;
                        do { id = "action" + std::to_string(serial++); }
                        while (std::any_of(actions.begin(), actions.end(), [&](const auto& a) { return a.at("id") == id; }));
                        actions.push_back(DefaultAction(id)); action = int(actions.size()) - 1; changed = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::BeginDisabled(actions.size() <= 1);
                    if (ImGui::Button("Remove action")) { actions.erase(actions.begin() + action); action = std::max(0, action - 1); changed = true; }
                    ImGui::EndDisabled();
                    action = std::clamp(action, 0, int(actions.size()) - 1);
                    auto& selected = actions[action];
                    ImGui::SeparatorText("Selected action");
                    changed |= JsonText("Stable action ID", selected, "id");
                    changed |= Number("Base weight", selected, "weight");
                    const auto mode = selected.at("mode").get<std::string>();
                    int modeIndex = mode == "mean" ? 1 : 0;
                    if (ImGui::Combo("Combine considerations", &modeIndex, "Product (zero vetoes)\0Weighted mean\0")) {
                        selected["mode"] = modeIndex ? "mean" : "product"; changed = true;
                    }
                    if (ImGui::CollapsingHeader("Commitment, interruption and cooldown")) {
                        changed |= Number("Minimum commitment (seconds)", selected, "minCommitment");
                        changed |= Number("Cooldown after exit (seconds)", selected, "cooldown");
                        bool interrupt = selected.at("interruptible").get<bool>();
                        if (ImGui::Checkbox("Can be interrupted", &interrupt)) { selected["interruptible"] = interrupt; changed = true; }
                    }
                    if (ImGui::CollapsingHeader("Required facts (must be positive)")) {
                        auto& required = selected.at("requiredFacts");
                        for (std::size_t i = 0; i < required.size(); ++i) {
                            ImGui::PushID(int(i)); auto fact = required[i].get<std::string>();
                            if (Text("Fact", fact)) { required[i] = fact; changed = true; }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Remove")) { required.erase(required.begin() + i); changed = true; ImGui::PopID(); break; }
                            ImGui::PopID();
                        }
                        if (required.size() < 32 && ImGui::Button("Add required fact")) { required.push_back("available"); changed = true; }
                    }
                    ImGui::SeparatorText("Scoring considerations");
                    auto& considerations = selected.at("considerations");
                    for (std::size_t i = 0; i < considerations.size(); ++i) {
                        auto& c = considerations[i];
                        ImGui::PushID(int(i));
                        if (ImGui::TreeNodeEx("consideration", ImGuiTreeNodeFlags_DefaultOpen, "%s", c.at("fact").get<std::string>().c_str())) {
                            changed |= JsonText("Fact name", c, "fact");
                            changed |= Number("Weight", c, "weight");
                            changed |= Number("Input minimum", c, "min");
                            changed |= Number("Input maximum", c, "max");
                            const auto curve = c.at("curve").get<std::string>();
                            int curveIndex = curve == "inverse" ? 1 : curve == "power" ? 2 : 0;
                            if (ImGui::Combo("Response", &curveIndex, "Linear\0Inverse\0Power\0")) {
                                c["curve"] = curveIndex == 1 ? "inverse" : curveIndex == 2 ? "power" : "linear"; changed = true;
                            }
                            if (curveIndex == 2) changed |= Number("Exponent", c, "exponent");
                            float values[33];
                            for (int j = 0; j < 33; ++j) {
                                const double x = j / 32.;
                                values[j] = float(curveIndex == 1 ? 1 - x : curveIndex == 2 ? std::pow(x, std::clamp(c.at("exponent").get<double>(), .01, 100.)) : x);
                            }
                            ImGui::PlotLines("Response curve", values, 33, 0, "Normalized input 0 → 1", 0, 1, {0, 65});
                            const bool remove = ImGui::SmallButton("Remove consideration");
                            ImGui::TreePop();
                            if (remove) { considerations.erase(considerations.begin() + i); changed = true; ImGui::PopID(); break; }
                        }
                        ImGui::PopID();
                    }
                    if (considerations.size() < 32 && ImGui::Button("Add consideration")) { considerations.push_back(DefaultConsideration()); changed = true; }
                    if (changed) { Remember(); definition = draft.dump(); }
                    try { (void)UtilityAgent::NormalizeDefinition(definition); }
                    catch (const std::exception& e) { ImGui::TextWrapped("Cannot save yet: %s", e.what()); }
                    ImGui::EndDisabled();
                }
            } else ImGui::TextWrapped("Create a definition, or choose an existing JSON default-export module. Your NPC script imports the module and supplies action implementations.");
            if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Live decisions",nullptr,requestedLive?ImGuiTabItemFlags_SetSelected:0)) {
            if (!state.runtime || !state.runtime->Scripts()) ImGui::TextWrapped("Press Play to inspect script-owned agents. Stopping Play releases their runtime state.");
            else {
                ImGui::Checkbox("Follow selected object",&followSelected);
                const auto filter=followSelected?state.utilityAISelected:state.utilityAIEntity;
                if(filter){ImGui::SameLine();ImGui::TextDisabled("Entity %llu",static_cast<unsigned long long>(filter));}
                else ImGui::TextDisabled("No entity filter: showing all agents");
                if(!followSelected&&filter&&ImGui::SmallButton("Show all agents"))state.utilityAIEntity=0;
                const auto agents = EditorAIAgentsForEntity(state.runtime->Scripts()->AIInspections(),filter);
                if (agents.empty()) ImGui::TextWrapped("No Utility AI agents match this object. A coordinator script owns its agents on the coordinator entity, not on guessed visual targets. Create an agent from a project script, then supply candidates and tick it during fixedUpdate.");
                else {
                    runtimeAgent = std::clamp(runtimeAgent, 0, int(agents.size()) - 1);
                    const auto label = [&](int i) { return agents[i].name + "  [entity " + std::to_string(agents[i].entity) + ", script " + std::to_string(agents[i].slot) + "]"; };
                    if (ImGui::BeginCombo("Agent", label(runtimeAgent).c_str())) {
                        for (int i = 0; i < int(agents.size()); ++i)
                            if (ImGui::Selectable(label(i).c_str(), runtimeAgent == i)) runtimeAgent = i;
                        ImGui::EndCombo();
                    }
                    const auto inspection = Json::parse(agents[runtimeAgent].json, nullptr, false);
                    if (inspection.is_discarded()) ImGui::TextWrapped("AI inspection unavailable: malformed diagnostic snapshot.");
                    else {
                        ImGui::TextWrapped("Decision: %s", inspection.value("reason", std::string{}).c_str());
                        ImGui::Text("Status: %s", inspection.value("status", std::string{}).c_str());
                        if (inspection.contains("selected")) ImGui::TextWrapped("Selected action / target: %s", inspection["selected"].dump().c_str());
                        ImGui::Text("Next evaluation: %.3f s; commitment: %.3f s",
                            inspection.value("untilEvaluation", 0.), inspection.value("commitmentRemaining", 0.));
                        if (inspection.contains("candidates") && ImGui::BeginTable("candidateScores", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
                            for (const auto* heading : {"Action", "Target", "Eligible", "Score", "Reason"}) ImGui::TableSetupColumn(heading);
                            ImGui::TableHeadersRow();
                            for (const auto& candidate : inspection["candidates"]) {
                                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(candidate.value("action", std::string{}).c_str());
                                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(candidate.value("target", std::string{}).c_str());
                                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(candidate.value("eligible", false) ? "Yes" : "No");
                                ImGui::TableSetColumnIndex(3); ImGui::Text("%.4g", candidate.value("score", 0.));
                                ImGui::TableSetColumnIndex(4); ImGui::TextWrapped("%s", candidate.value("reason", std::string{}).c_str());
                            }
                            ImGui::EndTable();
                        }
                        if (ImGui::CollapsingHeader("Consideration contributions, cooldowns and full snapshot")) JsonTree(inspection);
                        ImGui::TextDisabled("Read-only observation. Gameplay remains in the project's action scripts.");
                    }
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
