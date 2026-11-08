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
#include <fstream>
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
    ImGuiWindow *Properties() const {
        for (auto *window : ImGui::GetCurrentContext()->Windows)
            if (window->ParentWindow == Window() &&
                std::strstr(window->Name, "Inspector component properties"))
                return window;
        return nullptr;
    }
    bool FocusNative(ImGuiID id, ImRect &rectangle, ImGuiWindow *owner = nullptr) {
        if (!owner)
            owner = Window();
        ImGuiID previousId = 0;
        ImGuiWindow *previousWindow = nullptr;
        for (int i = 0; i < 64; ++i) {
            const auto *g = ImGui::GetCurrentContext();
            if (g->NavId == id && g->NavWindow == owner && g->NavCursorVisible) {
                rectangle =
                    ImGui::WindowRectRelToAbs(g->NavWindow, g->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
                return true;
            }
            const bool forwardTabStalled = g->NavId == previousId && g->NavWindow == previousWindow;
            previousId = g->NavId;
            previousWindow = g->NavWindow;
            const auto *child = Properties();
            if (child && owner == child && g->NavWindow == Window() && g->NavId == child->ChildId) {
                // Normal ImGui navigation enters a child scope with Enter;
                // Tab alone stays within the current focus scope.
                Key(ImGuiKey_Enter, true);
                Key(ImGuiKey_Enter, false);
            } else if (child && owner == Window() && g->NavWindow == child) {
                Key(ImGuiKey_Escape, true);
                Key(ImGuiKey_Escape, false);
            } else if (forwardTabStalled && g->NavWindow == owner) {
                // A direct fixture mode change can restore a child's previous,
                // now-disabled authored control. Normal Shift+Tab recovers from
                // that native remembered focus without resetting navigation.
                Key(ImGuiMod_Shift, true);
                Key(ImGuiKey_Tab, true);
                Key(ImGuiKey_Tab, false);
                Key(ImGuiMod_Shift, false);
            } else {
                Key(ImGuiKey_Tab, true);
                Key(ImGuiKey_Tab, false);
            }
        }
        return false;
    }
    void Click(ImVec2 at, int button = ImGuiMouseButton_Left) {
        ImGui::GetIO().AddMousePosEvent(at.x - 1, at.y);
        Frame();
        ImGui::GetIO().AddMousePosEvent(at.x, at.y);
        Frame();
        ImGui::GetIO().AddMouseButtonEvent(button, true);
        Frame();
        ImGui::GetIO().AddMouseButtonEvent(button, false);
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
    const auto item = popups.back().Window->GetID("Audio listener###Audio listener");
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
                      ("judas-inspector-properties-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
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
    if (!Check(CreateAndAttachEditorScript(ui.doc, ui.Object().id, project, assets, "InspectorProperties.js",
                                           record, error, source),
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
    const auto settings = ImHashStr("Script settings", 0, slotId);
    Check(!properties->StateStorage.GetBool(settings),
          "script settings are initially folded instead of overwhelming ordinary object editing");
    ui.Activate(settings, false);
    ui.Frame();
    Check(properties->StateStorage.GetBool(settings) && ui.Bytes() == before,
          "opening actual script settings reveals controls without editing authored content");
    ui.Activate(widget);
    if (!Check(ImGui::GetActiveID() == widget,
               "stacked script property preserves its original named native widget ID"))
        return;
    const auto field = ImGui::WindowRectRelToAbs(properties, properties->NavRectRel[ImGuiNavLayer_Main]);
    // The shared Surface owns an explicit 12px right inset. Its temporary
    // WorkRect is restored after drawing, so use that documented content edge.
    const float cardContentRight = properties->WorkRect.Max.x - 12.f * ImGui::GetIO().FontGlobalScale;
    const float available = cardContentRight - field.Min.x;
    Check(field.GetWidth() >= available - 2 && field.Max.x <= cardContentRight + 1 &&
              field.Max.x <= properties->ClipRect.Max.x + 1,
          "script value uses the readable available width inside a 293-pixel Inspector");
    const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
    const char *visibleLabel = "Maximum Walking Acceleration Metres Per Second Squared";
    const float labelHeight = ImGui::CalcTextSize(visibleLabel, nullptr, false, available).y;
    unsigned labelVertices = 0;
    bool labelInside = true;
    for (const auto &vertex : properties->DrawList->VtxBuffer)
        if (vertex.col == textColor &&
            vertex.pos.y >= field.Min.y - labelHeight - ImGui::GetStyle().ItemSpacing.y - 2 &&
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
void ModelInspectionShortcut() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
                      ("judas-inspector-model-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{root};
    Project project;
    AssetDatabase assets;
    std::string error;
    if (!Check(Project::CreateNew(root.string(), "Model shortcut", project, error),
               "model inspection fixture creates a real project"))
        return;
    const auto mesh = fs::path(project.AssetsDir()) / "model.obj";
    std::ofstream(mesh) << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    assets.Scan(project.RootDir(), project.AssetsDir());
    AssetRecord record;
    if (!Check(assets.Track(mesh.string(), record, error),
               "model inspection fixture tracks a real source mesh"))
        return;
    InspectorFrames ui;
    ui.state.project = &project;
    ui.state.assets = &assets;
    ui.Object().render = SceneRenderComponent{};
    ui.Object().render->shape = SceneShape::Mesh;
    ui.Object().render->meshAsset = record.id;
    ui.state.showAssetBrowser = false;
    const auto before = ui.Bytes();
    const auto generation = ui.doc.Generation();
    ui.Frame();
    ui.Frame();
    ImRect modelAction;
    if (!Check(ui.FocusNative(ui.Window()->GetID("Inspect model"), modelAction),
               "actual model action is reachable through ordinary keyboard navigation"))
        return;
    ui.Click(modelAction.GetCenter());
    Check(ui.state.showAssetBrowser && ui.state.selectAssetDetails && ui.state.revealModelStructure &&
              ui.state.browserSelection == record.id,
          "actual Inspect model action opens details for the exact source mesh and requests model structure");
    Check(ui.Bytes() == before && ui.doc.Generation() == generation && !ui.doc.CanUndo() &&
              !ui.doc.EditInProgress(),
          "inspecting a model does not create scene children or change authored content/history");
    ui.state.showAssetBrowser = ui.state.selectAssetDetails = ui.state.revealModelStructure = false;
    ui.state.mode = EditorMode::Play;
    ui.Frame();
    auto modelInPlay = modelAction.GetCenter();
    modelInPlay.y += ImGui::GetTextLineHeightWithSpacing(); // The read-only notice occupies one line.
    ui.Click(modelInPlay);
    Check(!ui.state.showAssetBrowser && !ui.state.revealModelStructure && ui.Bytes() == before,
          "Play's model action is disabled until the authored Asset Browser can actually display it");
}
void ReadOnlyDisclosureNavigation() {
    InspectorFrames ui;
    ui.Object().body = SceneBodyComponent{};
    ui.Frame();
    ui.Frame();
    auto *properties = ui.Properties();
    if (!Check(properties != nullptr, "read-only navigation uses the actual component panel"))
        return;
    const auto header = ImHashStr("Body", 0, properties->GetID("Body"));
    const auto before = ui.Bytes();
    ui.state.mode = EditorMode::Play;
    ui.Frame();
    ImRect playHeader;
    if (!Check(ui.FocusNative(header, playHeader, properties),
               "Play component disclosure remains keyboard focusable"))
        return;
    ui.Click(playHeader.GetCenter());
    Check(properties->StateStorage.GetBool(header) && ui.Bytes() == before,
          "actual Play pointer expands component details without changing the authored scene");

    ui.state.mode = EditorMode::Edit;
    ui.Frame();
    ImRect enabledField;
    if (!Check(ui.FocusNative(properties->GetID("Collider enabled"), enabledField, properties),
               "enabled authored toggle is reached through real keyboard navigation"))
        return;
    const bool originalEnabled = ui.Object().body->enabled;
    ui.Click(enabledField.GetCenter());
    Check(ui.Object().body->enabled != originalEnabled && ui.doc.CanUndo(),
          "same pointer stimulus changes the enabled collider control in Edit");
    ui.doc.Undo();
    ui.Frame();
    ImRect editHeader;
    if (!Check(ui.FocusNative(header, editHeader, properties),
               "actual header rectangle is available before Play comparison"))
        return;
    const auto ellipsis = [](ImRect headerRect) {
        return ImVec2(headerRect.Max.x - 16, headerRect.Min.y + 14);
    };
    ui.Click(ellipsis(editHeader));
    Check(!ImGui::GetCurrentContext()->OpenPopupStack.empty(),
          "Edit pointer reaches the real component-actions button and opens its menu");
    if (!ImGui::GetCurrentContext()->OpenPopupStack.empty()) {
        ui.Frame(); // Popup auto-size resolves before observing its actual row bounds.
        auto *popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        ui.Click(popup->InnerClipRect.GetCenter());
        Check(!ui.Object().body, "enabled component menu genuinely removes the body");
        ui.doc.Undo();
    }
    ui.Frame();
    ImRect addComponent;
    if (!Check(ui.FocusNative(ui.Window()->GetID("##add"), addComponent),
               "Edit Add component is reachable as an actual native combo"))
        return;
    ui.Click(addComponent.GetCenter());
    Check(!ImGui::GetCurrentContext()->OpenPopupStack.empty(),
          "Edit pointer opens the actual Add component menu");
    ui.Key(ImGuiKey_Escape, true);
    ui.Key(ImGuiKey_Escape, false);

    const auto locked = ui.Bytes();
    const auto generation = ui.doc.Generation();
    // The fixture changes mode directly instead of handing focus to the Game
    // viewport as the application's actual Play workflow does. The child can
    // therefore remember its component-actions button, now disabled. Exercise
    // ordinary keyboard recovery as well as the read-only pointer safeguards.
    ui.state.mode = EditorMode::Play;
    ui.Frame();
    if (!Check(ui.FocusNative(header, playHeader, properties),
               "Play header stays navigable after authoring controls lock"))
        return;
    const ImVec2 shift{playHeader.Min.x - editHeader.Min.x, playHeader.Min.y - editHeader.Min.y};
    auto toggleInPlay = enabledField.GetCenter();
    toggleInPlay.x += shift.x;
    toggleInPlay.y += shift.y;
    ui.Click(toggleInPlay);
    Check(ui.Object().body && ui.Object().body->enabled == originalEnabled,
          "actual Play pointer cannot change the authored collider toggle");
    ui.Click(ellipsis(playHeader));
    Check(ImGui::GetCurrentContext()->OpenPopupStack.empty() && ui.Object().body.has_value(),
          "component removal action stays disabled while Play disclosure is enabled");
    ui.Click(addComponent.GetCenter());
    Check(ImGui::GetCurrentContext()->OpenPopupStack.empty(),
          "Add component stays disabled during Play despite enabled disclosure navigation");
    ui.Click(playHeader.GetCenter(), ImGuiMouseButton_Right);
    Check(!ImGui::GetCurrentContext()->OpenPopupStack.empty(),
          "read-only header context still exposes its component action menu");
    if (!ImGui::GetCurrentContext()->OpenPopupStack.empty()) {
        ui.Frame(); // Popup auto-size resolves before observing its actual row bounds.
        auto *popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        ui.Click(popup->InnerClipRect.GetCenter());
        ui.Key(ImGuiKey_Escape, true);
        ui.Key(ImGuiKey_Escape, false);
    }
    Check(ui.Object().body.has_value() && ui.Bytes() == locked && ui.doc.Generation() == generation &&
              !ui.doc.EditInProgress(),
          "disabled Remove menu and other Play pointers preserve exact authored bytes/history");
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
        ModelInspectionShortcut();
        ReadOnlyDisclosureNavigation();
    } catch (const std::exception &error) {
        Check(false, error.what());
    }
    std::printf("Inspector workflow: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
