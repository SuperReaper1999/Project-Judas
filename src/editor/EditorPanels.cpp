#include "EditorPanels.h"

#include <cstring>
#include <glm/gtc/quaternion.hpp>

#include "imgui.h"

namespace {

// Snapshot-before / commit-after helpers around ImGui widgets. Drag and
// input widgets are "active" while being edited, so the snapshot is taken
// on activation and committed when the widget deactivates having changed
// something; instant widgets (checkbox, combo, button) commit at once.
void TrackEdit(EditorDocument& doc) {
    if (ImGui::IsItemActivated()) doc.BeginEdit();
    if (ImGui::IsItemDeactivatedAfterEdit()) doc.CommitEdit();
    else if (ImGui::IsItemDeactivated()) doc.CancelEdit();
}

bool DragVec3(EditorDocument& doc, const char* label, glm::vec3& value, float speed = 0.05f) {
    const bool changed = ImGui::DragFloat3(label, &value.x, speed, 0.0f, 0.0f, "%.4g");
    TrackEdit(doc);
    return changed;
}
bool DragScalar(EditorDocument& doc, const char* label, float& value, float speed = 0.01f, float min = 0.0f,
                float max = 0.0f) {
    const bool changed = ImGui::DragFloat(label, &value, speed, min, max, "%.4g");
    TrackEdit(doc);
    return changed;
}
bool DragInt(EditorDocument& doc, const char* label, int& value, int min = 0, int max = 100000) {
    const bool changed = ImGui::DragInt(label, &value, 1.0f, min, max);
    TrackEdit(doc);
    return changed;
}
bool ColorEdit(EditorDocument& doc, const char* label, glm::vec3& value) {
    const bool changed = ImGui::ColorEdit3(label, &value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    TrackEdit(doc);
    return changed;
}
bool Checkbox(EditorDocument& doc, const char* label, bool& value) {
    bool v = value;
    if (ImGui::Checkbox(label, &v)) {
        doc.BeginEdit();
        value = v;
        doc.CommitEdit();
        return true;
    }
    return false;
}
template <typename Enum>
bool Combo(EditorDocument& doc, const char* label, Enum& value, const char* const* names, int count) {
    int index = static_cast<int>(value);
    if (ImGui::Combo(label, &index, names, count)) {
        doc.BeginEdit();
        value = static_cast<Enum>(index);
        doc.CommitEdit();
        return true;
    }
    return false;
}
bool TextField(EditorDocument& doc, const char* label, std::string& value) {
    char buffer[512];
    std::strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    const bool changed = ImGui::InputText(label, buffer, sizeof(buffer));
    if (ImGui::IsItemActivated()) doc.BeginEdit();
    if (changed) value = buffer;
    if (ImGui::IsItemDeactivatedAfterEdit()) doc.CommitEdit();
    else if (ImGui::IsItemDeactivated()) doc.CancelEdit();
    return changed;
}
bool AssetCombo(EditorDocument& doc, const char* label, std::string& value, const std::vector<std::string>& assets,
                bool allowNone) {
    bool changed = false;
    if (ImGui::BeginCombo(label, value.empty() ? "(none)" : value.c_str())) {
        if (allowNone && ImGui::Selectable("(none)", value.empty())) {
            doc.BeginEdit(); value.clear(); doc.CommitEdit(); changed = true;
        }
        for (const std::string& asset : assets) {
            if (ImGui::Selectable(asset.c_str(), asset == value)) {
                doc.BeginEdit(); value = asset; doc.CommitEdit(); changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

template <typename T>
bool ComponentHeader(EditorDocument& doc, const char* name, std::optional<T>& component) {
    ImGui::PushID(name);
    bool open = ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
    if (ImGui::SmallButton("Remove")) {
        doc.BeginEdit();
        component.reset();
        doc.CommitEdit();
        open = false;
    }
    ImGui::PopID();
    return open && component.has_value();
}

const char* const kShapeNames[] = {"box", "sphere", "compound", "mesh", "terrain"};
const char* const kBodyShapeNames[] = {"box", "sphere", "compound", "(mesh: not a body shape)", "terrain"};
const char* const kMotionNames[] = {"static", "dynamic"};
const char* const kGravityKindNames[] = {"radial", "uniform"};
const char* const kRegionNames[] = {"sphere", "box"};
const char* const kLightKindNames[] = {"point", "spot"};
const char* const kVehicleGravityNames[] = {"local", "celestial"};
const char* const kViewNames[] = {"third-person", "first-person"};

void DrawTransform(EditorDocument& doc, SceneObject& o) {
    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return;
    DragVec3(doc, "Position", o.transform.position);
    // Rotation is edited as yaw/pitch/roll degrees for humans; the
    // authored value stays a quaternion.
    glm::vec3 euler = glm::degrees(glm::eulerAngles(glm::normalize(o.transform.rotation)));
    if (ImGui::DragFloat3("Rotation (deg)", &euler.x, 0.5f, 0.0f, 0.0f, "%.3g")) {
        o.transform.rotation = glm::normalize(glm::quat(glm::radians(euler)));
    }
    TrackEdit(doc);
    DragVec3(doc, "Scale", o.transform.scale, 0.01f);
    ImGui::TextDisabled("Scale applies to mesh rendering only.");
}

void DrawRender(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    if (!ComponentHeader(doc, "Render", o.render)) return;
    SceneRenderComponent& r = *o.render;
    Combo(doc, "Shape", r.shape, kShapeNames, 5);
    if (r.shape == SceneShape::Box) DragVec3(doc, "Half extents", r.halfExtents, 0.01f);
    if (r.shape == SceneShape::Sphere) DragScalar(doc, "Radius", r.radius, 0.01f, 0.001f, 100000.0f);
    ColorEdit(doc, "Color", r.color);
    DragScalar(doc, "Alpha", r.alpha, 0.01f, 0.0f, 1.0f);
    if (r.shape == SceneShape::Mesh) {
        AssetCombo(doc, "Mesh", r.meshPath, state.modelAssets, false);
        AssetCombo(doc, "Texture", r.texturePath, state.textureAssets, true);
    }
    if (r.shape == SceneShape::Compound) {
        ColorEdit(doc, "Wall color", r.secondaryColor);
        DragScalar(doc, "Wall alpha", r.secondaryAlpha, 0.01f, 0.0f, 1.0f);
        ImGui::TextDisabled("Geometry comes from the compound body.");
    }
    if (r.shape == SceneShape::Terrain) ImGui::TextDisabled("Geometry comes from the terrain body.");
}

void DrawBody(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    if (!ComponentHeader(doc, "Body", o.body)) return;
    SceneBodyComponent& b = *o.body;
    Combo(doc, "Motion", b.motion, kMotionNames, 2);
    Combo(doc, "Collider", b.shape, kBodyShapeNames, 5);
    if (b.shape == SceneShape::Box) DragVec3(doc, "Half extents##body", b.halfExtents, 0.01f);
    if (b.shape == SceneShape::Sphere) DragScalar(doc, "Radius##body", b.radius, 0.01f, 0.001f, 100000.0f);
    if (b.shape == SceneShape::Terrain) AssetCombo(doc, "Surface", b.terrainSurface, state.terrainSurfaces, false);
    if (b.shape == SceneShape::Compound) {
        ImGui::Text("%zu child boxes", b.compoundBoxes.size());
        for (std::size_t i = 0; i < b.compoundBoxes.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            DragVec3(doc, "Center", b.compoundBoxes[i].localCenter, 0.005f);
            DragVec3(doc, "Half extents", b.compoundBoxes[i].halfExtents, 0.005f);
            ImGui::PopID();
        }
        if (ImGui::SmallButton("Add child box")) {
            doc.BeginEdit();
            b.compoundBoxes.push_back(CompoundBox{glm::vec3(0.0f), glm::vec3(0.1f)});
            doc.CommitEdit();
        }
    }
    if (b.motion == SceneBodyMotion::Dynamic) {
        DragScalar(doc, "Mass (kg)", b.mass, 0.1f, 0.001f, 1.0e30f);
        DragVec3(doc, "Initial velocity", b.initialLinearVelocity, 0.05f);
        Checkbox(doc, "Pickable (G/H)", b.pickable);
    }
    DragScalar(doc, "Friction", b.friction, 0.01f, 0.0f, 5.0f);
    DragScalar(doc, "Restitution", b.restitution, 0.01f, 0.0f, 1.0f);
}

void DrawGravity(EditorDocument& doc, SceneObject& o) {
    if (!ComponentHeader(doc, "Gravity region", o.gravity)) return;
    SceneGravityComponent& g = *o.gravity;
    Combo(doc, "Kind", g.kind, kGravityKindNames, 2);
    DragScalar(doc, "Magnitude (m/s^2)", g.magnitude, 0.01f, 0.0f, 1000.0f);
    Combo(doc, "Region", g.regionShape, kRegionNames, 2);
    if (g.regionShape == SceneRegionShape::Sphere) DragScalar(doc, "Region radius", g.regionRadius, 0.1f, 0.0f, 1.0e6f);
    else DragVec3(doc, "Region half extents", g.regionHalfExtents, 0.1f);
    ImGui::TextDisabled(g.kind == SceneGravityKind::Radial ? "Pulls toward this object's position."
                                                            : "Pulls along this object's local -Y.");
    ImGui::TextDisabled("Earlier objects win where regions overlap.");
}

void DrawLight(EditorDocument& doc, SceneObject& o) {
    if (!ComponentHeader(doc, "Light", o.light)) return;
    SceneLightComponent& l = *o.light;
    Combo(doc, "Kind##light", l.kind, kLightKindNames, 2);
    ColorEdit(doc, "Color##light", l.color);
    DragScalar(doc, "Range", l.range, 0.1f, 0.0f, 10000.0f);
    if (l.kind == SceneLightKind::Spot) {
        DragScalar(doc, "Inner cone (deg)", l.innerConeDegrees, 0.5f, 0.0f, 89.0f);
        DragScalar(doc, "Outer cone (deg)", l.outerConeDegrees, 0.5f, 0.0f, 89.0f);
        ImGui::TextDisabled("A spot light faces this object's local -Z.");
    }
}

void DrawDoor(EditorDocument& doc, SceneObject& o) {
    if (!ComponentHeader(doc, "Door", o.door)) return;
    DragVec3(doc, "Hinge axis (local)", o.door->localHingeAxis, 0.01f);
    DragScalar(doc, "Open angle (deg)", o.door->openAngleDegrees, 0.5f, 0.0f, 180.0f);
    DragScalar(doc, "Angular speed (deg/s)", o.door->angularSpeedDegreesPerSecond, 1.0f, 0.0f, 3600.0f);
    ImGui::TextDisabled("Needs a box Render for the panel; hinge at the object's transform.");
}

void DrawLightSwitch(EditorDocument& doc, SceneObject& o) {
    if (!ComponentHeader(doc, "Light switch", o.lightSwitch)) return;
    SceneLightSwitchComponent& s = *o.lightSwitch;
    DragVec3(doc, "Hinge axis (local)##sw", s.localHingeAxis, 0.01f);
    DragScalar(doc, "Toggle angle (deg)", s.toggleAngleDegrees, 0.5f, 0.0f, 180.0f);
    DragScalar(doc, "Angular speed (deg/s)##sw", s.angularSpeedDegreesPerSecond, 1.0f, 0.0f, 3600.0f);
    DragVec3(doc, "Lamp offset (local)", s.lampLocalOffset, 0.05f);
    ColorEdit(doc, "Lamp color", s.lampColor);
    DragScalar(doc, "Lamp range", s.lampRange, 0.1f, 0.0f, 1000.0f);
}

void DrawVehicle(EditorDocument& doc, SceneObject& o) {
    if (!ComponentHeader(doc, "Vehicle", o.vehicle)) return;
    SceneVehicleComponent& v = *o.vehicle;
    Combo(doc, "Gravity source", v.gravity, kVehicleGravityNames, 2);
    Checkbox(doc, "Headlight", v.headlight);
    Checkbox(doc, "Navigation lights", v.navigationLights);
    DragScalar(doc, "Drag coefficient", v.dragCoefficient, 0.01f, 0.0f, 10.0f);
    Checkbox(doc, "Start with pilot attached", v.initialPilotAttached);
    ImGui::TextDisabled("Needs a dynamic box Body. One vehicle per scene.");
}

void DrawCelestial(EditorDocument& doc, SceneObject& o) {
    if (!ComponentHeader(doc, "Celestial", o.celestial)) return;
    DragScalar(doc, "Gravitational parameter (static, m^3/s^2)", o.celestial->gravitationalParameter, 10.0f, 0.0f, 1.0e30f);
    DragScalar(doc, "Operator thrust (N)", o.celestial->operatorThrustForce, 1.0e9f, 0.0f, 1.0e30f);
    ImGui::TextDisabled("Dynamic: joins pairwise Newtonian gravity by its mass.");
}

void DrawAtmosphere(EditorDocument& doc, SceneObject& o) {
    if (!ComponentHeader(doc, "Atmosphere", o.atmosphere)) return;
    SceneAtmosphereComponent& a = *o.atmosphere;
    DragScalar(doc, "Reference radius (m)", a.referenceRadius, 0.1f, 0.001f, 1.0e9f);
    DragScalar(doc, "Top radius (m)", a.topRadius, 0.1f, 0.001f, 1.0e9f);
    DragScalar(doc, "Reference density (kg/m^3)", a.referenceDensity, 0.001f, 0.0f, 1000.0f);
    DragScalar(doc, "Polytropic exponent", a.polytropicExponent, 0.001f, 1.001f, 1.999f);
    DragScalar(doc, "Oxidizer mass fraction", a.oxidizerMassFraction, 0.001f, 0.0f, 1.0f);
    DragScalar(doc, "Reference temperature (K)", a.referenceTemperatureKelvin, 1.0f, 1.0f, 10000.0f);
    ImGui::TextDisabled("Needs a static Celestial gravitational parameter.");
}

void DrawCombustible(EditorDocument& doc, SceneObject& o) {
    if (!ComponentHeader(doc, "Combustible", o.combustible)) return;
    SceneCombustibleComponent& c = *o.combustible;
    DragScalar(doc, "Heat capacity (J/K)", c.heatCapacityJPerK, 1.0f, 0.001f, 1.0e9f);
    DragScalar(doc, "Fuel mass (kg)", c.initialFuelMassKg, 0.001f, 0.0f, 1.0e6f);
    DragScalar(doc, "Ignition temperature (K)", c.ignitionTemperatureK, 1.0f, 0.0f, 10000.0f);
    DragScalar(doc, "Max fuel rate (kg/s)", c.maximumFuelRateKgPerSecond, 0.0001f, 0.0f, 1000.0f);
    DragScalar(doc, "Radiative area (m^2)", c.radiativeAreaSquareMeters, 0.01f, 0.0f, 1.0e6f);
    DragScalar(doc, "Retained heat fraction", c.retainedCombustionHeatFraction, 0.01f, 0.0f, 1.0f);
}

void DrawFluidVolume(EditorDocument& doc, SceneObject& o) {
    if (!ComponentHeader(doc, "Fluid volume", o.fluidVolume)) return;
    SceneFluidVolumeComponent& f = *o.fluidVolume;
    DragScalar(doc, "Particle spacing (m)", f.spacing, 0.001f, 0.001f, 100.0f);
    int count[3] = {f.countX, f.countY, f.countZ};
    if (ImGui::DragInt3("Lattice count", count, 1.0f, 0, 1000)) {
        f.countX = count[0]; f.countY = count[1]; f.countZ = count[2];
    }
    TrackEdit(doc);
    Checkbox(doc, "Emitter (hold B)", f.emitter);
    if (f.emitter) {
        DragVec3(doc, "Emitter offset (local)", f.emitterLocalOffset, 0.05f);
        DragInt(doc, "Max particles", f.maxParticles, 0, 100000);
    }
    ImGui::TextDisabled("Lattice grows along local +Y from the object.");
}

void DrawPlayerStart(EditorDocument& doc, SceneObject& o) {
    if (!ComponentHeader(doc, "Player start", o.playerStart)) return;
    DragScalar(doc, "Yaw (deg)", o.playerStart->yawDegrees, 0.5f, -360.0f, 360.0f);
    Combo(doc, "View", o.playerStart->view, kViewNames, 2);
}

void DrawAddComponentMenu(EditorDocument& doc, SceneObject& o) {
    if (!ImGui::BeginCombo("##add", "Add component...")) return;
    const auto option = [&](const char* label, auto& slot, auto make) {
        if (slot.has_value()) return;
        if (ImGui::Selectable(label)) {
            doc.BeginEdit();
            slot = make();
            doc.CommitEdit();
        }
    };
    option("Render", o.render, [] { return SceneRenderComponent{}; });
    option("Body", o.body, [] { return SceneBodyComponent{}; });
    option("Gravity region", o.gravity, [] { return SceneGravityComponent{}; });
    option("Light", o.light, [] { return SceneLightComponent{}; });
    option("Door", o.door, [] { return SceneDoorComponent{}; });
    option("Light switch", o.lightSwitch, [] { return SceneLightSwitchComponent{}; });
    option("Vehicle", o.vehicle, [] { return SceneVehicleComponent{}; });
    option("Celestial", o.celestial, [] { return SceneCelestialComponent{}; });
    option("Atmosphere", o.atmosphere, [] { return SceneAtmosphereComponent{}; });
    option("Combustible", o.combustible, [] { return SceneCombustibleComponent{}; });
    option("Fluid volume", o.fluidVolume, [] { return SceneFluidVolumeComponent{}; });
    option("Player start", o.playerStart, [] { return ScenePlayerStartComponent{}; });
    ImGui::EndCombo();
}

}  // namespace

void DrawEditorMainMenu(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests) {
    if (!ImGui::BeginMainMenuBar()) return;
    const bool editing = state.mode == EditorMode::Edit;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New scene", nullptr, false, editing)) requests.newScene = true;
        if (ImGui::MenuItem("Open...", nullptr, false, editing)) requests.open = true;
        if (ImGui::MenuItem("Save", "Ctrl+S", false, editing)) requests.save = true;
        if (ImGui::MenuItem("Save As...", nullptr, false, editing)) requests.saveAs = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) requests.quit = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editing && doc.CanUndo())) requests.undo = true;
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, editing && doc.CanRedo())) requests.redo = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Focus selection", "F", false, doc.Selected() != kInvalidSceneObjectId)) requests.focusSelection = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Create", editing)) {
        const auto item = [&](const char* label, const char* kind) {
            if (ImGui::MenuItem(label)) { requests.createKind = kind; requests.createPosition = state.cameraFocus; }
        };
        item("Empty object", "empty");
        item("Static box", "box");
        item("Static sphere", "sphere");
        item("Dynamic box", "dynamic-box");
        item("Dynamic sphere", "dynamic-sphere");
        item("Mesh", "mesh");
        item("Point light", "point-light");
        item("Spot light", "spot-light");
        item("Player start", "player-start");
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (editing) {
        if (ImGui::MenuItem("Play")) requests.play = true;
    } else {
        if (ImGui::MenuItem("Stop")) requests.stop = true;
        ImGui::TextDisabled(state.playPaused ? "PLAYING (paused - Escape resumes)" : "PLAYING - Escape pauses and frees the mouse");
    }
    ImGui::EndMainMenuBar();
}

void DrawHierarchyPanel(EditorDocument& doc, EditorPanelState& state, EditorRequests& requests) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 24.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hierarchy")) { ImGui::End(); return; }
    const bool editing = state.mode == EditorMode::Edit;
    Scene& scene = doc.GetScene();
    ImGui::TextDisabled("%zu objects", scene.Objects().size());
    ImGui::Separator();
    SceneObjectId toDelete = kInvalidSceneObjectId;
    int move = 0;
    SceneObjectId moveId = kInvalidSceneObjectId;
    ImGui::BeginChild("list", ImVec2(0.0f, -32.0f));
    for (const SceneObject& o : scene.Objects()) {
        ImGui::PushID(static_cast<int>(o.id));
        const bool selected = o.id == doc.Selected();
        std::string label = o.name.empty() ? "(unnamed)" : o.name;
        label += "##" + std::to_string(o.id);
        if (ImGui::Selectable(label.c_str(), selected)) doc.Select(o.id);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("id %llu", static_cast<unsigned long long>(o.id));
        if (editing && ImGui::BeginPopupContextItem()) {
            doc.Select(o.id);
            if (ImGui::MenuItem("Move up")) { move = -1; moveId = o.id; }
            if (ImGui::MenuItem("Move down")) { move = 1; moveId = o.id; }
            if (ImGui::MenuItem("Delete")) toDelete = o.id;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (editing) {
        if (ImGui::Button("Delete selected") && doc.Selected() != kInvalidSceneObjectId) toDelete = doc.Selected();
        ImGui::SameLine();
        if (ImGui::Button("Focus")) requests.focusSelection = true;
    }
    if (toDelete != kInvalidSceneObjectId) {
        doc.BeginEdit();
        scene.DestroyObject(toDelete);
        doc.CommitEdit();
        if (doc.Selected() == toDelete) doc.Select(kInvalidSceneObjectId);
        state.status = "Deleted object";
    }
    if (move != 0) {
        doc.BeginEdit();
        scene.MoveObject(moveId, move);
        doc.CommitEdit();
    }
    ImGui::End();
}

void DrawInspectorPanel(EditorDocument& doc, EditorPanelState& state) {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 380.0f, 24.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380.0f, ImGui::GetIO().DisplaySize.y - 24.0f - 28.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inspector")) { ImGui::End(); return; }
    SceneObject* o = doc.SelectedObject();
    if (!o) {
        ImGui::TextDisabled("Nothing selected. Click an object in the viewport or the hierarchy.");
        ImGui::End();
        return;
    }
    if (state.mode == EditorMode::Play) {
        ImGui::TextDisabled("Authored values are read-only while playing.");
        ImGui::BeginDisabled();
    }
    ImGui::Text("id %llu", static_cast<unsigned long long>(o->id));
    TextField(doc, "Name", o->name);
    DrawTransform(doc, *o);
    DrawRender(doc, *o, state);
    DrawBody(doc, *o, state);
    DrawGravity(doc, *o);
    DrawLight(doc, *o);
    DrawDoor(doc, *o);
    DrawLightSwitch(doc, *o);
    DrawVehicle(doc, *o);
    DrawCelestial(doc, *o);
    DrawAtmosphere(doc, *o);
    DrawCombustible(doc, *o);
    DrawFluidVolume(doc, *o);
    DrawPlayerStart(doc, *o);
    ImGui::Separator();
    DrawAddComponentMenu(doc, *o);
    if (state.mode == EditorMode::Play) ImGui::EndDisabled();
    ImGui::End();
}

void DrawSceneSettingsPanel(EditorDocument& doc, EditorPanelState& state) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 448.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene")) { ImGui::End(); return; }
    if (state.mode == EditorMode::Play) ImGui::BeginDisabled();
    SceneSettings& s = doc.GetScene().Settings();
    TextField(doc, "Name##scene", s.name);
    double origin[3] = {s.worldOrigin.x, s.worldOrigin.y, s.worldOrigin.z};
    if (ImGui::InputScalarN("World origin (m)", ImGuiDataType_Double, origin, 3, nullptr, nullptr, "%.6g")) {
        s.worldOrigin = glm::dvec3(origin[0], origin[1], origin[2]);
    }
    TrackEdit(doc);
    DragVec3(doc, "Sun direction", s.sunDirection, 0.01f);
    ColorEdit(doc, "Sun color", s.sunColor);
    ColorEdit(doc, "Ambient", s.ambientColor);
    DragScalar(doc, "Fluid scale", s.fluidScale, 0.1f, 0.01f, 1000.0f);
    if (state.mode == EditorMode::Play) ImGui::EndDisabled();
    ImGui::End();
}

void DrawAssetPanel(EditorDocument& doc, EditorPanelState& state) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 672.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280.0f, 96.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Assets")) { ImGui::End(); return; }
    ImGui::TextDisabled("Models (%zu)", state.modelAssets.size());
    for (const std::string& m : state.modelAssets) {
        ImGui::BulletText("%s", m.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select a Mesh render component to assign this model.");
    }
    ImGui::TextDisabled("Textures (%zu)", state.textureAssets.size());
    for (const std::string& t : state.textureAssets) ImGui::BulletText("%s", t.c_str());
    ImGui::TextDisabled("Terrain surfaces (%zu)", state.terrainSurfaces.size());
    for (const std::string& t : state.terrainSurfaces) ImGui::BulletText("%s", t.c_str());
    (void)doc;
    ImGui::End();
}

void DrawStatusBar(EditorDocument& doc, EditorPanelState& state) {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - 26.0f));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 26.0f));
    ImGui::Begin("##status", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("%s%s  |  %s  |  %s", doc.Path().empty() ? "(unsaved scene)" : doc.Path().c_str(),
                doc.IsDirty() ? " *" : "", state.mode == EditorMode::Edit ? "Edit" : "Play",
                state.status.c_str());
    if (!state.runtimeInfo.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", state.runtimeInfo.c_str()); }
    ImGui::End();
}

SceneObjectId CreateObjectOfKind(EditorDocument& doc, const std::string& kind, const glm::vec3& position,
                                 const std::string& defaultMeshPath) {
    doc.BeginEdit();
    Scene& scene = doc.GetScene();
    SceneObject& o = scene.CreateObject(kind == "empty" ? "Empty" : kind == "box" ? "Box"
                                        : kind == "sphere" ? "Sphere" : kind == "dynamic-box" ? "Dynamic box"
                                        : kind == "dynamic-sphere" ? "Dynamic sphere" : kind == "mesh" ? "Mesh"
                                        : kind == "point-light" ? "Point light" : kind == "spot-light" ? "Spot light"
                                        : kind == "player-start" ? "Player start" : "Object");
    o.transform.position = position;
    if (kind == "box" || kind == "dynamic-box") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Box;
        o.body = SceneBodyComponent{};
        o.body->shape = SceneShape::Box;
        o.body->motion = kind == "box" ? SceneBodyMotion::Static : SceneBodyMotion::Dynamic;
        o.body->mass = 5.0f;
        o.body->pickable = kind == "dynamic-box";
    } else if (kind == "sphere" || kind == "dynamic-sphere") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Sphere;
        o.body = SceneBodyComponent{};
        o.body->shape = SceneShape::Sphere;
        o.body->motion = kind == "sphere" ? SceneBodyMotion::Static : SceneBodyMotion::Dynamic;
        o.body->mass = 4.0f;
        o.body->pickable = kind == "dynamic-sphere";
    } else if (kind == "mesh") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Mesh;
        o.render->meshPath = defaultMeshPath;
        o.render->color = glm::vec3(1.0f);
    } else if (kind == "point-light" || kind == "spot-light") {
        o.light = SceneLightComponent{};
        o.light->kind = kind == "point-light" ? SceneLightKind::Point : SceneLightKind::Spot;
        o.light->color = glm::vec3(3.0f, 2.8f, 2.4f);
    } else if (kind == "player-start") {
        o.playerStart = ScenePlayerStartComponent{};
    }
    const SceneObjectId id = o.id;
    doc.CommitEdit();
    doc.Select(id);
    return id;
}
