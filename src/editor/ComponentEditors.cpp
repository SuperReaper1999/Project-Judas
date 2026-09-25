#include "ComponentEditors.h"

#include <glm/gtc/quaternion.hpp>

#include "imgui.h"

#include "AssetDatabase.h"
#include "EditorPanels.h"
#include "EditorWidgets.h"

namespace {
const char* const kShapeNames[] = {"box", "sphere", "compound", "mesh", "terrain"};
const char* const kBodyShapeNames[] = {"box", "sphere", "compound", "(mesh: not a body shape)", "terrain"};
const char* const kMotionNames[] = {"static", "dynamic"};
const char* const kGravityKindNames[] = {"radial", "uniform"};
const char* const kRegionNames[] = {"sphere", "box"};
const char* const kLightKindNames[] = {"point", "spot"};
const char* const kVehicleGravityNames[] = {"local", "celestial"};
const char* const kViewNames[] = {"third-person", "first-person"};

// The inspector's asset fields: a combo over the project's assets of one
// type (name shown, id stored), a drop target for the Asset Browser's
// drag payload, and an honest status line for the stored id.
void AssetField(EditorDocument& doc, const char* label, std::string& assetId, AssetType type, bool allowNone,
                EditorPanelState& state) {
    std::vector<std::string> labels, values;
    if (state.assets) {
        for (const auto& [id, record] : state.assets->Records()) {
            if (record.type != type) continue;
            labels.push_back(record.relativePath + (record.missing ? "  [missing]" : ""));
            values.push_back(id);
        }
    }
    LabelledCombo(doc, label, assetId, labels, values, allowNone);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragPayload)) {
            const std::string droppedId(static_cast<const char*>(payload->Data), payload->DataSize);
            const AssetRecord* record = state.assets ? state.assets->Find(droppedId) : nullptr;
            if (record && record->type == type) {
                doc.BeginEdit();
                assetId = droppedId;
                doc.CommitEdit();
            } else {
                state.status = std::string("Dropped asset is not a ") + AssetTypeName(type);
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (assetId.empty()) return;
    const AssetRecord* record = state.assets ? state.assets->Find(assetId) : nullptr;
    if (!record) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "  unknown asset id %s", assetId.c_str());
    } else if (record->missing) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "  file missing: %s", record->relativePath.c_str());
    } else {
        ImGui::TextDisabled("  id %s", assetId.c_str());
    }
}

void DrawRender(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    SceneRenderComponent& r = *o.render;
    Combo(doc, "Shape", r.shape, kShapeNames, 5);
    if (r.shape == SceneShape::Box) DragVec3(doc, "Half extents", r.halfExtents, 0.01f);
    if (r.shape == SceneShape::Sphere) DragScalar(doc, "Radius", r.radius, 0.01f, 0.001f, 100000.0f);
    ColorEdit(doc, "Color", r.color);
    DragScalar(doc, "Alpha", r.alpha, 0.01f, 0.0f, 1.0f);
    if (r.shape == SceneShape::Mesh) {
        AssetField(doc, "Mesh", r.meshAsset, AssetType::Mesh, false, state);
        AssetField(doc, "Texture", r.textureAsset, AssetType::Texture, true, state);
        ImGui::TextDisabled("Drag an asset from the Asset Browser onto a field.");
    }
    if (r.shape == SceneShape::Compound) {
        ColorEdit(doc, "Wall color", r.secondaryColor);
        DragScalar(doc, "Wall alpha", r.secondaryAlpha, 0.01f, 0.0f, 1.0f);
        ImGui::TextDisabled("Geometry comes from the compound body.");
    }
    if (r.shape == SceneShape::Terrain) ImGui::TextDisabled("Geometry comes from the terrain body.");
}

void DrawBody(EditorDocument& doc, SceneObject& o, EditorPanelState& state) {
    SceneBodyComponent& b = *o.body;
    Combo(doc, "Motion", b.motion, kMotionNames, 2);
    Combo(doc, "Collider", b.shape, kBodyShapeNames, 5);
    if (b.shape == SceneShape::Box) DragVec3(doc, "Half extents##body", b.halfExtents, 0.01f);
    if (b.shape == SceneShape::Sphere) DragScalar(doc, "Radius##body", b.radius, 0.01f, 0.001f, 100000.0f);
    if (b.shape == SceneShape::Terrain) StringCombo(doc, "Surface", b.terrainSurface, state.terrainSurfaces, false);
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
        Checkbox(doc, "Managed by fidelity policy (M29)", b.managed);
        if (!b.managed) ImGui::TextDisabled("Unmanaged: always fully simulated; the policy never touches it.");
    }
    DragScalar(doc, "Friction", b.friction, 0.01f, 0.0f, 5.0f);
    DragScalar(doc, "Restitution", b.restitution, 0.01f, 0.0f, 1.0f);
}

void DrawGravity(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
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

void DrawLight(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
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

void DrawDoor(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    DragVec3(doc, "Hinge axis (local)", o.door->localHingeAxis, 0.01f);
    DragScalar(doc, "Open angle (deg)", o.door->openAngleDegrees, 0.5f, 0.0f, 180.0f);
    DragScalar(doc, "Angular speed (deg/s)", o.door->angularSpeedDegreesPerSecond, 1.0f, 0.0f, 3600.0f);
    ImGui::TextDisabled("Needs a box Render for the panel; hinge at the object's transform.");
}

void DrawLightSwitch(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    SceneLightSwitchComponent& s = *o.lightSwitch;
    DragVec3(doc, "Hinge axis (local)##sw", s.localHingeAxis, 0.01f);
    DragScalar(doc, "Toggle angle (deg)", s.toggleAngleDegrees, 0.5f, 0.0f, 180.0f);
    DragScalar(doc, "Angular speed (deg/s)##sw", s.angularSpeedDegreesPerSecond, 1.0f, 0.0f, 3600.0f);
    DragVec3(doc, "Lamp offset (local)", s.lampLocalOffset, 0.05f);
    ColorEdit(doc, "Lamp color", s.lampColor);
    DragScalar(doc, "Lamp range", s.lampRange, 0.1f, 0.0f, 1000.0f);
}

void DrawVehicle(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    SceneVehicleComponent& v = *o.vehicle;
    Combo(doc, "Gravity source", v.gravity, kVehicleGravityNames, 2);
    Checkbox(doc, "Headlight", v.headlight);
    Checkbox(doc, "Navigation lights", v.navigationLights);
    DragScalar(doc, "Drag coefficient", v.dragCoefficient, 0.01f, 0.0f, 10.0f);
    Checkbox(doc, "Start with pilot attached", v.initialPilotAttached);
    ImGui::TextDisabled("Needs a dynamic box Body. One vehicle per scene.");
}

void DrawCelestial(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    DragScalar(doc, "Gravitational parameter (static, m^3/s^2)", o.celestial->gravitationalParameter, 10.0f, 0.0f, 1.0e30f);
    DragScalar(doc, "Operator thrust (N)", o.celestial->operatorThrustForce, 1.0e9f, 0.0f, 1.0e30f);
    ImGui::TextDisabled("Dynamic: joins pairwise Newtonian gravity by its mass.");
}

void DrawAtmosphere(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    SceneAtmosphereComponent& a = *o.atmosphere;
    DragScalar(doc, "Reference radius (m)", a.referenceRadius, 0.1f, 0.001f, 1.0e9f);
    DragScalar(doc, "Top radius (m)", a.topRadius, 0.1f, 0.001f, 1.0e9f);
    DragScalar(doc, "Reference density (kg/m^3)", a.referenceDensity, 0.001f, 0.0f, 1000.0f);
    DragScalar(doc, "Polytropic exponent", a.polytropicExponent, 0.001f, 1.001f, 1.999f);
    DragScalar(doc, "Oxidizer mass fraction", a.oxidizerMassFraction, 0.001f, 0.0f, 1.0f);
    DragScalar(doc, "Reference temperature (K)", a.referenceTemperatureKelvin, 1.0f, 1.0f, 10000.0f);
    ImGui::TextDisabled("Needs a static Celestial gravitational parameter.");
}

void DrawCombustible(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    SceneCombustibleComponent& c = *o.combustible;
    DragScalar(doc, "Heat capacity (J/K)", c.heatCapacityJPerK, 1.0f, 0.001f, 1.0e9f);
    DragScalar(doc, "Fuel mass (kg)", c.initialFuelMassKg, 0.001f, 0.0f, 1.0e6f);
    DragScalar(doc, "Ignition temperature (K)", c.ignitionTemperatureK, 1.0f, 0.0f, 10000.0f);
    DragScalar(doc, "Max fuel rate (kg/s)", c.maximumFuelRateKgPerSecond, 0.0001f, 0.0f, 1000.0f);
    DragScalar(doc, "Radiative area (m^2)", c.radiativeAreaSquareMeters, 0.01f, 0.0f, 1.0e6f);
    DragScalar(doc, "Retained heat fraction", c.retainedCombustionHeatFraction, 0.01f, 0.0f, 1.0f);
}

void DrawFluidVolume(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
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

void DrawPlayerStart(EditorDocument& doc, SceneObject& o, EditorPanelState&) {
    DragScalar(doc, "Yaw (deg)", o.playerStart->yawDegrees, 0.5f, -360.0f, 360.0f);
    Combo(doc, "View", o.playerStart->view, kViewNames, 2);
    ImGui::TextDisabled("Exactly one object may carry a player start.");
}

template <typename T>
ComponentEditor Make(const char* name, char indicator, std::optional<T> SceneObject::*member,
                     void (*draw)(EditorDocument&, SceneObject&, EditorPanelState&)) {
    // Function pointers cannot capture, so the member pointer is threaded
    // through a static per-instantiation slot; each component type is a
    // distinct T, so each instantiation has its own slot.
    static std::optional<T> SceneObject::*slot = nullptr;
    slot = member;
    ComponentEditor editor;
    editor.name = name;
    editor.indicator = indicator;
    editor.has = [](const SceneObject& o) { return (o.*slot).has_value(); };
    editor.add = [](SceneObject& o) { o.*slot = T{}; };
    editor.remove = [](SceneObject& o) { (o.*slot).reset(); };
    editor.draw = draw;
    return editor;
}
}  // namespace

const std::vector<ComponentEditor>& ComponentEditorRegistry() {
    static const std::vector<ComponentEditor> registry = {
        Make<SceneRenderComponent>("Render", 'R', &SceneObject::render, DrawRender),
        Make<SceneBodyComponent>("Body", 'B', &SceneObject::body, DrawBody),
        Make<SceneGravityComponent>("Gravity region", 'G', &SceneObject::gravity, DrawGravity),
        Make<SceneLightComponent>("Light", 'L', &SceneObject::light, DrawLight),
        Make<SceneDoorComponent>("Door", 'D', &SceneObject::door, DrawDoor),
        Make<SceneLightSwitchComponent>("Light switch", 'S', &SceneObject::lightSwitch, DrawLightSwitch),
        Make<SceneVehicleComponent>("Vehicle", 'V', &SceneObject::vehicle, DrawVehicle),
        Make<SceneCelestialComponent>("Celestial", 'C', &SceneObject::celestial, DrawCelestial),
        Make<SceneAtmosphereComponent>("Atmosphere", 'A', &SceneObject::atmosphere, DrawAtmosphere),
        Make<SceneCombustibleComponent>("Combustible", 'F', &SceneObject::combustible, DrawCombustible),
        Make<SceneFluidVolumeComponent>("Fluid volume", 'W', &SceneObject::fluidVolume, DrawFluidVolume),
        Make<ScenePlayerStartComponent>("Player start", 'P', &SceneObject::playerStart, DrawPlayerStart),
    };
    return registry;
}

void DrawTransformEditor(EditorDocument& doc, SceneObject& o) {
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

std::string ComponentIndicators(const SceneObject& object) {
    std::string out;
    for (const ComponentEditor& editor : ComponentEditorRegistry()) {
        if (editor.has(object)) out += editor.indicator;
    }
    return out;
}
