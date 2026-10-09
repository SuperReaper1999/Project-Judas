#include <cstdio>
#include <string>

#include <glm/gtc/quaternion.hpp>

#include "editor/EditorWidgets.h"
#include "imgui.h"

namespace {
int checks = 0;
int failures = 0;

void Check(bool condition, const char* description) {
    ++checks;
    if (!condition) ++failures;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", description);
}

bool Exact(glm::quat actual, glm::quat expected) {
    return actual.w == expected.w && actual.x == expected.x &&
           actual.y == expected.y && actual.z == expected.z;
}

// Only Dear ImGui's core is needed: a built font atlas, a fixed window,
// and ordinary input events. No SDL window, renderer, or GL context.
class WidgetFrames {
public:
    WidgetFrames() {
        context = ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(800, 480);
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        doc.NewScene();
        id = doc.GetScene().CreateObject("Original").id;
        // The opposite quaternion sign represents the same orientation,
        // but Undo must restore the exact authored representation.
        Object().transform.rotation = -glm::normalize(glm::quat(glm::radians(glm::vec3(17, -23, 11))));
    }

    ~WidgetFrames() { ImGui::DestroyContext(context); }

    SceneObject& Object() { return *doc.GetScene().Find(id); }

    void Frame() {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(340, 260), ImGuiCond_Always);
        ImGui::Begin("Widget lifecycle", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
        const auto availableWidth = ImGui::GetContentRegionAvail().x;
        ImGui::PushID("Rotation (deg)");
        ImGui::PushID(0);
        expectedRotationId = ImGui::GetID("");
        ImGui::PopID();
        ImGui::PopID();
        rotationChanged = DragRotation(doc, "Rotation (deg)", Object().transform.rotation);
        rotationId = ImGui::GetItemID();
        rotationFillsRow = ImGui::GetItemRectMax().x - ImGui::GetItemRectMin().x >= availableWidth - 2;
        rotationPoint = ImVec2(ImGui::GetItemRectMin().x + 40,
                              (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f);
        expectedTextId = ImGui::GetID("Runtime document name");
        textChanged = TextField(doc, "Runtime document name", Object().name);
        textId = ImGui::GetItemID();
        textFillsRow = ImGui::GetItemRectMax().x - ImGui::GetItemRectMin().x >= availableWidth - 2;
        textPoint = ImVec2(ImGui::GetItemRectMin().x + 40,
                          (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f);
        ImGui::Button("Finish edit");
        finishPoint = ImVec2((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                            (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f);
        ImGui::End();
        ImGui::Render();
    }

    void Move(ImVec2 position) {
        ImGui::GetIO().AddMousePosEvent(position.x, position.y);
        Frame();
    }

    void Mouse(bool down) {
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, down);
        Frame();
    }

    void Key(ImGuiKey key, bool down) {
        ImGui::GetIO().AddKeyEvent(key, down);
        Frame();
    }

    void Type(const char* text) {
        ImGui::GetIO().AddInputCharactersUTF8(text);
        Frame();
    }

    EditorDocument doc;
    ImVec2 rotationPoint, textPoint, finishPoint;
    bool rotationChanged = false;
    bool textChanged = false;
    bool rotationFillsRow = false;
    bool textFillsRow = false;
    ImGuiID rotationId = 0, expectedRotationId = 0;
    ImGuiID textId = 0, expectedTextId = 0;

private:
    ImGuiContext* context = nullptr;
    SceneObjectId id = kInvalidSceneObjectId;
};

void RotationUndo() {
    WidgetFrames ui;
    ui.Frame();
    ui.Frame();
    const auto original = ui.Object().transform.rotation;
    const auto generation = ui.doc.Generation();
    const auto start = ui.rotationPoint;
    ui.Move(start);
    ui.Mouse(true);
    Check(ui.rotationId == ui.expectedRotationId,
          "stacked rotation control preserves the original first-component widget ID");
    Check(ui.rotationFillsRow,
          "rotation components use the available width of a 340-pixel inspector");
    Check(ui.doc.EditInProgress() && !ui.doc.CanUndo(),
          "rotation press starts one pending edit without committing history");
    ui.Move(ImVec2(start.x + 35, start.y));
    Check(ui.rotationChanged && !Exact(ui.Object().transform.rotation, original),
          "real pointer drag changes the authored quaternion");
    Check(ui.doc.EditInProgress() && ui.doc.Generation() == generation &&
              Exact(ui.doc.EditBaseline().Find(ui.Object().id)->transform.rotation, original),
          "first changed drag frame keeps the complete original quaternion as baseline");
    ui.Move(ImVec2(start.x + 70, start.y));
    const auto changed = ui.Object().transform.rotation;
    ui.Mouse(false);
    ui.Frame();
    Check(!ui.doc.EditInProgress() && ui.doc.CanUndo() &&
              ui.doc.Generation() == generation + 1 && ui.doc.ValidationError().empty(),
          "rotation release commits exactly one valid scene edit");
    ui.doc.Undo();
    Check(Exact(ui.Object().transform.rotation, original) && !ui.doc.CanUndo(),
          "one Undo restores the exact quaternion from before the first drag change");
    ui.doc.Redo();
    Check(Exact(ui.Object().transform.rotation, changed),
          "rotation Redo restores the final drag result");
}

void TextUndo() {
    WidgetFrames ui;
    ui.Frame();
    ui.Frame();
    const auto original = ui.Object().name;
    const auto generation = ui.doc.Generation();
    ui.Move(ui.textPoint);
    ui.Mouse(true);
    ui.Mouse(false);
    Check(ui.textId == ui.expectedTextId,
          "stacked TextField preserves the original labelled input ID");
    Check(ui.textFillsRow,
          "TextField uses the available width of a 340-pixel inspector");
    ui.Key(ImGuiKey_End, true);
    ui.Key(ImGuiKey_End, false);
    ui.Type(" Room");
    Check(ui.textChanged && ui.Object().name == "Original Room",
          "real text input changes multiple characters in the shared TextField");
    ui.Type(" 42");
    Check(ui.textChanged && ui.Object().name == "Original Room 42" &&
              ui.doc.EditInProgress() && !ui.doc.CanUndo() &&
              ui.doc.Generation() == generation,
          "successive text input frames remain one pending edit");
    ui.Move(ui.finishPoint);
    ui.Mouse(true);
    ui.Mouse(false);
    ui.Frame();
    Check(!ui.doc.EditInProgress() && ui.doc.CanUndo() &&
              ui.doc.Generation() == generation + 1 && ui.doc.ValidationError().empty(),
          "leaving TextField commits all characters as one valid scene edit");
    ui.doc.Undo();
    Check(ui.Object().name == original && !ui.doc.CanUndo(),
          "one Undo restores the name before the entire typing gesture");
    ui.doc.Redo();
    Check(ui.Object().name == "Original Room 42",
          "text Redo restores the complete multi-character edit");
}
}

int main() {
    RotationUndo();
    TextUndo();
    std::printf("Editor widget lifecycle: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
