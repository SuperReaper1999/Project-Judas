#include "SceneSerialization.h"
#include "AssetDatabase.h"
#include "Project.h"
#include "editor/EditorDocument.h"
#include "editor/EditorPanels.h"
#include "editor/EditorScriptWorkflow.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <glm/gtc/quaternion.hpp>

namespace {
unsigned checks = 0, failures = 0;
bool Check(bool condition, const char *message) {
    ++checks;
    failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
    std::fflush(stdout);
    return condition;
}

bool Exact(glm::quat actual, glm::quat expected) {
    return actual.w == expected.w && actual.x == expected.x && actual.y == expected.y &&
           actual.z == expected.z;
}

// The actual inspector, with its cards, compact fields, component child and
// pinned component menu. Only the native UI backends are absent: input reaches
// the real widgets through ImGui's ordinary keyboard events and nav activation.
class InspectorFrames {
  public:
    InspectorFrames() {
        context = ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {1000, 900};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        unsigned char *pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        state.dockingEnabled = true;
        state.showInspector = true;
        doc.NewScene();
        id = doc.GetScene().CreateObject("Inspector workflow fixture").id;
        Object().transform.position = {1.25f, -2.5f, 3.75f};
        // This sign is physically equivalent, but is authored information that
        // an unchanged frame or Undo must preserve exactly.
        Object().transform.rotation = -glm::normalize(glm::quat(glm::radians(glm::vec3(17, -23, 11))));
        doc.Select(id);
        Frame();
        Frame();
    }
    ~InspectorFrames() {
        ImGui::DestroyContext(context);
    }
    InspectorFrames(const InspectorFrames &) = delete;
    InspectorFrames &operator=(const InspectorFrames &) = delete;

    SceneObject &Object() {
        return *doc.GetScene().Find(id);
    }
    ImGuiWindow *Window() const {
        return ImGui::FindWindowByName("Inspector");
    }

    void Frame() {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({40, 40}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({width, 800}, ImGuiCond_Always);
        DrawInspectorPanel(doc, state);
        ImGui::Render();
    }
    ImGuiID Axis(const char *label, int axis) const {
        // CompactAxes preserves the original label / empty vector / axis /
        // empty scalar IDs. Hashing an empty string preserves its seed.
        return ImHashData(&axis, sizeof(axis), Window()->GetID(label));
    }
    void Activate(ImGuiID widget, bool text = true) {
        ImGui::ActivateItemByID(widget);
        if (text)
            ImGui::GetCurrentContext()->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
        Frame();
    }
    void Key(ImGuiKey key, bool pressed) {
        ImGui::GetIO().AddKeyEvent(key, pressed);
        Frame();
    }
    void ReplaceText(const char *text) {
        auto &io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_A, true);
        Frame();
        io.AddKeyEvent(ImGuiKey_A, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        Frame();
        io.AddInputCharactersUTF8(text);
        Frame();
    }
    void Confirm() {
        Key(ImGuiKey_Enter, true);
        Key(ImGuiKey_Enter, false);
        Frame();
    }
    std::string Bytes() const {
        std::string result;
        if (!SaveSceneToString(doc.GetScene(), result))
            throw std::runtime_error("Inspector fixture scene did not serialize");
        return result;
    }

    EditorDocument doc;
    EditorPanelState state;
    float width = 380;

  private:
    ImGuiContext *context = nullptr;
    SceneObjectId id = kInvalidSceneObjectId;
};

void IdleFrames() {
    InspectorFrames ui;
    const auto before = ui.Bytes();
    const auto rotation = ui.Object().transform.rotation;
    const auto generation = ui.doc.Generation();
    for (int i = 0; i < 8; ++i)
        ui.Frame();
    Check(ui.Bytes() == before && Exact(ui.Object().transform.rotation, rotation),
          "actual inspector idle frames preserve exact scene bytes and negative quaternion sign");
    Check(ui.doc.Generation() == generation && !ui.doc.EditInProgress() && !ui.doc.CanUndo(),
          "drawing compact transform fields creates no pending edit or undo history");
}

void PositionAxis(int axis, const char *text, float expected) {
    InspectorFrames ui;
    const auto before = ui.Bytes();
    const auto position = ui.Object().transform.position;
    const auto rotation = ui.Object().transform.rotation;
    const auto generation = ui.doc.Generation();
    const auto widget = ui.Axis("Position", axis);
    ui.Activate(widget);
    if (!Check(ImGui::TempInputIsActive(widget), "actual compact position scalar enters keyboard input mode"))
        return;
    Check(ui.doc.EditInProgress() && !ui.doc.CanUndo(),
          "position keyboard activation opens one document transaction");
    ui.ReplaceText(text);
    bool correct = std::abs(ui.Object().transform.position[axis] - expected) < 1.e-6f;
    for (int other = 0; other < 3; ++other)
        if (other != axis)
            correct = correct && ui.Object().transform.position[other] == position[other];
    Check(correct && Exact(ui.Object().transform.rotation, rotation),
          "typed position updates only the intended XYZ scalar in the actual scene");
    ui.Confirm();
    Check(!ui.doc.EditInProgress() && ui.doc.CanUndo() && ui.doc.Generation() == generation + 1 &&
              ui.doc.ValidationError().empty(),
          "confirming position commits exactly one valid document edit");
    ui.doc.Undo();
    Check(ui.Bytes() == before && !ui.doc.CanUndo(),
          "one Undo restores exact scene bytes before the compact position edit");
}

void RotationUndo() {
    InspectorFrames ui;
    const auto before = ui.Bytes();
    const auto rotation = ui.Object().transform.rotation;
    const auto generation = ui.doc.Generation();
    const auto widget = ui.Axis("Rotation (deg)", 1);
    ui.Activate(widget);
    if (!Check(ImGui::TempInputIsActive(widget), "actual compact rotation scalar enters keyboard input mode"))
        return;
    Check(ui.doc.EditInProgress() &&
              Exact(ui.doc.EditBaseline().Find(ui.Object().id)->transform.rotation, rotation),
          "rotation transaction snapshots the exact negative-sign authored quaternion");
    ui.ReplaceText("-42");
    const auto expected = glm::normalize(glm::quat(glm::radians(glm::vec3(17, -42, 11))));
    Check(!Exact(ui.Object().transform.rotation, rotation) &&
              std::abs(glm::dot(ui.Object().transform.rotation, expected)) > .99999f,
          "keyboard editing rotation changes actual orientation to the requested angle");
    ui.Confirm();
    Check(!ui.doc.EditInProgress() && ui.doc.CanUndo() && ui.doc.Generation() == generation + 1 &&
              ui.doc.ValidationError().empty(),
          "confirming rotation commits exactly one valid document edit");
    ui.doc.Undo();
    Check(Exact(ui.Object().transform.rotation, rotation) && ui.Bytes() == before && !ui.doc.CanUndo(),
          "one Undo restores the complete original quaternion including its negative sign");
}

void NameUndo() {
    InspectorFrames ui;
    const auto before = ui.Bytes();
    const auto generation = ui.doc.Generation();
    const auto widget = ui.Window()->GetID("Name");
    ui.Activate(widget);
    if (!Check(ImGui::GetActiveID() == widget,
               "compact primary Name field preserves its original labelled input ID"))
        return;
    Check(ui.doc.EditInProgress() && !ui.doc.CanUndo(),
          "activating the actual Name field starts one document transaction");
    ui.ReplaceText("Renamed through the actual inspector");
    Check(ui.Object().name == "Renamed through the actual inspector",
          "ordinary text input updates the selected object's real name");
    ui.Confirm();
    Check(!ui.doc.EditInProgress() && ui.doc.CanUndo() && ui.doc.Generation() == generation + 1 &&
              ui.doc.ValidationError().empty(),
          "confirming Name commits exactly one valid document edit");
    ui.doc.Undo();
    Check(ui.Bytes() == before && !ui.doc.CanUndo(),
          "one Undo restores exact authored bytes before the primary Name edit");
}

// Unlike ActivateItemByID, native Tab cannot target a disabled control. The
// direct internal activation API deliberately bypasses nav eligibility, so it
// is useful for targeting editable fields but is not a realistic read-only test.
bool NativeKeyboardAttempts(InspectorFrames &ui, const std::string &before) {
    ImGui::SetWindowFocus("Inspector");
    ui.Frame();
    for (int attempt = 0; attempt < 12; ++attempt) {
        ui.Key(ImGuiKey_Tab, true);
        ui.Key(ImGuiKey_Tab, false);
        ui.ReplaceText("999");
        ui.Confirm();
        if (ui.Bytes() != before)
            return true;
    }
    return false;
}

void ReadOnlyPlay() {
    {
        InspectorFrames editable;
        const auto before = editable.Bytes();
        Check(NativeKeyboardAttempts(editable, before),
              "same native Tab/select-all/type/confirm sequence edits enabled inspector controls");
    }
    InspectorFrames ui;
    const auto before = ui.Bytes();
    const auto generation = ui.doc.Generation();
    ui.state.mode = EditorMode::Play;
    ui.Frame();
    const auto widget = ui.Axis("Position", 0);
    const bool changed = NativeKeyboardAttempts(ui, before);
    Check(!ImGui::TempInputIsActive(widget), "Play's disabled authored position rejects activation");
    Check(!changed && ui.Bytes() == before && ui.doc.Generation() == generation && !ui.doc.EditInProgress() &&
              !ui.doc.CanUndo(),
          "keyboard input cannot modify authored scene or history through the Play inspector");
}

void AddComponent() {
    InspectorFrames ui;
    const auto before = ui.Bytes();
    ui.Activate(ui.Window()->GetID("##add"), false);
    ui.Frame(); // Let the actual combo popup complete its auto-size frame.
    auto &popups = ImGui::GetCurrentContext()->OpenPopupStack;
    if (!Check(!popups.empty() && popups.back().Window && popups.back().Window->Active,
               "pinned Add component control opens its actual component menu"))
        return;
    const auto item = popups.back().Window->GetID("Audio listener");
    ui.Activate(item, false);
    Check(ui.Object().audioListener.has_value() && ui.doc.CanUndo() && ui.doc.ValidationError().empty(),
          "choosing Audio listener in the actual component menu adds it to the scene");
    ui.doc.Undo();
    Check(ui.Bytes() == before && !ui.doc.CanUndo(),
          "one Undo removes the added component and restores exact authored scene");
}

void ScriptPropertyLayoutAndUndo() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
        ("judas-inspector-properties-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path path;
        ~Cleanup() { std::error_code ignored; fs::remove_all(path, ignored); }
    } cleanup{root};
    Project project;
    AssetDatabase assets;
    std::string error;
    if (!Check(Project::CreateNew(root.string(), "Inspector properties", project, error),
               "script property fixture creates a real project"))
        return;
    assets.Scan(project.RootDir(), project.AssetsDir());
    InspectorFrames ui;
    AssetRecord record;
    const char *source =
        "export const properties={maximumWalkingAccelerationMetresPerSecondSquared:{type:'number',default:3}};\n"
        "export default class InspectorProperties {}\n";
    if (!Check(CreateAndAttachEditorScript(ui.doc, ui.Object().id, project, assets,
                                           "InspectorProperties.js", record, error, source),
               "script property fixture uses real create/register/attach authoring"))
        return;
    ui.width = 293;
    ui.state.project = &project;
    ui.state.assets = &assets;
    ImGui::GetStyle().Colors[ImGuiCol_TextDisabled] = {.91f, .19f, .73f, 1};
    ui.Frame();
    ui.Frame();
    ImGuiWindow *properties = nullptr;
    for (auto *window : ImGui::GetCurrentContext()->Windows)
        if (window->ParentWindow == ui.Window() &&
            std::strstr(window->Name, "Inspector component properties"))
            properties = window;
    if (!Check(properties != nullptr, "actual inspector owns the script-property child panel"))
        return;
    const char *name = "maximumWalkingAccelerationMetresPerSecondSquared";
    const int slot = 0;
    const auto slotId = ImHashData(&slot, sizeof(slot), properties->ID);
    const auto widget = ImHashStr(name, 0, slotId);
    const auto before = ui.Bytes();
    const auto generation = ui.doc.Generation();
    ui.Activate(widget);
    if (!Check(ImGui::GetActiveID() == widget,
               "stacked script property preserves its original named native widget ID"))
        return;
    const auto relative = properties->NavRectRel[ImGuiNavLayer_Main];
    const ImRect field{{relative.Min.x + properties->Pos.x, relative.Min.y + properties->Pos.y},
                       {relative.Max.x + properties->Pos.x, relative.Max.y + properties->Pos.y}};
    const float available = properties->WorkRect.Max.x - field.Min.x;
    Check(field.GetWidth() >= available - 2 && field.Max.x <= properties->ClipRect.Max.x + 1,
          "script value uses the readable available width inside a 293-pixel Inspector");
    const auto textColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const float labelHeight = ImGui::CalcTextSize(name, nullptr, false, available).y;
    unsigned labelVertices = 0;
    bool labelInside = true;
    for (const auto &vertex : properties->DrawList->VtxBuffer)
        if (vertex.col == textColor && vertex.pos.y >= field.Min.y - labelHeight - ImGui::GetStyle().ItemSpacing.y - 2 &&
            vertex.pos.y < field.Min.y) {
            ++labelVertices;
            labelInside = labelInside && vertex.pos.x >= properties->ClipRect.Min.x &&
                          vertex.pos.x <= properties->ClipRect.Max.x;
        }
    Check(labelHeight > ImGui::GetFontSize() && labelVertices >= std::strlen(name) * 4 && labelInside,
          "long script-property label wraps above its value with every glyph inside the panel");
    ui.ReplaceText("7.5");
    ui.Confirm();
    Check(ui.Object().scripts[0].properties.find("7.5") != std::string::npos &&
              ui.doc.Generation() == generation + 1 && ui.doc.ValidationError().empty(),
          "actual script-property text entry updates serialized properties in one existing edit");
    ui.doc.Undo();
    Check(ui.Bytes() == before, "one Undo restores exact scene bytes before the script-property edit");
}
} // namespace

int main() {
    try {
        IdleFrames();
        PositionAxis(0, "12.5", 12.5f);
        PositionAxis(1, "-8.25", -8.25f);
        PositionAxis(2, "0.125", .125f);
        RotationUndo();
        NameUndo();
        ReadOnlyPlay();
        AddComponent();
        ScriptPropertyLayoutAndUndo();
    } catch (const std::exception &error) {
        Check(false, error.what());
    }
    std::printf("Inspector workflow: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
