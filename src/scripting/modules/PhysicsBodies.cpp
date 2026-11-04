#include "../../AssetDatabase.h"
#include "../../ResourceManager.h"
#include "../../RuntimeWorld.h"
#include "../BindingContext.h"
#include "../BindingRegistry.h"
#include "../BindingValues.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "../../CollisionAsset.h"

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandlePhysicsBodies(BindingContext &context, const std::string &op, int argc,
                            JSValueConst *argv) {
    auto *s = &context;
    auto *c = context.js;
    auto &world = *s->world;
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };
    auto readTransform = [&](JSValueConst value, SceneTransform &t) {
        auto p = JS_GetPropertyStr(c, value, "position"),
             r = JS_GetPropertyStr(c, value, "rotation"),
             scale = JS_GetPropertyStr(c, value, "scale");
        bool ok = JS_IsUndefined(p) || ReadVec(c, p, t.position);
        if (!JS_IsUndefined(r))
            ok = ok && Number(c, r, "w", t.rotation.w) && Number(c, r, "x", t.rotation.x) &&
                 Number(c, r, "y", t.rotation.y) && Number(c, r, "z", t.rotation.z) &&
                 std::isfinite(glm::dot(t.rotation, t.rotation)) &&
                 glm::dot(t.rotation, t.rotation) > 1e-12f;
        if (!JS_IsUndefined(scale))
            ok =
                ok && ReadVec(c, scale, t.scale) && t.scale.x > 0 && t.scale.y > 0 && t.scale.z > 0;
        JS_FreeValue(c, p);
        JS_FreeValue(c, r);
        JS_FreeValue(c, scale);
        return ok;
    };

    auto namedLayerMask = [&](JSValueConst list, const CategoryRegistry &registry,
                              CategoryMask &mask) {
        if (JS_IsNull(list)) {
            mask = kAllCategories;
            return true;
        }
        if (!JS_IsArray(list))
            return false;
        auto length = JS_GetPropertyStr(c, list, "length");
        uint32_t count = 0;
        bool valid = JS_ToUint32(c, &count, length) == 0 && count <= 64;
        JS_FreeValue(c, length);
        mask = 0;
        for (uint32_t i = 0; valid && i < count; ++i) {
            auto item = JS_GetPropertyUint32(c, list, i);
            const int index = JS_IsString(item) ? registry.Find(String(c, item)) : -1;
            JS_FreeValue(c, item);
            valid = index >= 0;
            if (valid)
                mask |= CategoryBit(index);
        }
        return valid;
    };

    EntityId id = 0;
    try {
        auto text = String(c, arg(1));
        size_t end;
        id = std::stoull(text, &end);
        if (end != text.size())
            id = 0;
    } catch (...) {
        id = 0;
    }
    const auto *definition = world.RuntimeDefinition(id);
    if (!definition)
        return JS_ThrowReferenceError(c, "stale or invalid entity %llu", (unsigned long long)id);

    if (op == "entityGravityState") {
        auto o = JS_NewObject(c);
        const auto &selection = definition->gravitySelection;
        const char *mode = !selection                                         ? "spatial"
                           : selection->mode == GravitySelection::Mode::Field ? "field"
                                                                              : "uniform";
        JS_SetPropertyStr(c, o, "mode", JS_NewString(c, mode));
        JS_SetPropertyStr(c, o, "available", JS_NewBool(c, world.GravitySelectionAvailable(id)));
        auto source =
            selection && selection->mode == GravitySelection::Mode::Field ? selection->source : 0;
        JS_SetPropertyStr(c, o, "sourceId",
                          world.RuntimeDefinition(source)
                              ? JS_NewString(c, std::to_string(source).c_str())
                              : JS_NULL);
        auto p = world.PresentedTransform(id, definition->transform, 1).position;
        JS_SetPropertyStr(c, o, "acceleration", Vec(c, world.SampleEntityGravity(id, p)));
        return o;
    }

    if (op == "entityGravitySelect" || op == "entityGravityUniform" || op == "entityGravityClear") {
        std::optional<GravitySelection> selection;
        std::string error;
        if (op != "entityGravityClear") {
            selection = GravitySelection{};
            if (op == "entityGravityUniform") {
                if (!ReadVec(c, arg(2), selection->acceleration))
                    return JS_ThrowTypeError(c, "finite acceleration {x,y,z} required");
            } else {
                selection->mode = GravitySelection::Mode::Field;
                try {
                    auto text = String(c, arg(2));
                    size_t end;
                    selection->source = std::stoull(text, &end);
                    if (end != text.size())
                        selection->source = 0;
                } catch (...) {
                    selection->source = 0;
                }
                if (!world.RuntimeDefinition(selection->source))
                    return JS_ThrowReferenceError(c, "stale gravity source");
            }
        }
        return world.SetGravitySelection(id, selection, error)
                   ? JS_TRUE
                   : JS_ThrowTypeError(c, "%s", error.c_str());
    }

    if (op == "physicalMaterialInfo" || op == "physicalMaterialSet") {
        std::string asset;
        float friction = 0, restitution = 0;
        auto body = world.RuntimeBody(id);
        if (!world.Physics().GetPhysicalMaterial(body, asset, friction, restitution))
            return op == "physicalMaterialInfo"
                       ? JS_NULL
                       : JS_ThrowReferenceError(c, "unavailable physical body");
        if (op == "physicalMaterialSet") {
            std::string chosen;
            PhysicalMaterial material{friction, restitution};
            const PhysicalMaterial *override = nullptr;
            if (!JS_IsNull(arg(2))) {
                if (!JS_IsString(arg(2)))
                    return JS_ThrowTypeError(c, "physical material asset or null required");
                chosen = String(c, arg(2));
            }
            if (!chosen.empty()) {
                std::string error;
                auto loaded = world.Resources()
                                  ? world.Resources()->RequirePhysicalMaterial(chosen, error)
                                  : nullptr;
                if (!loaded)
                    return JS_ThrowTypeError(c, "%s", error.c_str());
                material = *loaded;
            }
            for (auto item : {std::pair<const char *, float *>{"friction", &material.friction},
                              {"restitution", &material.restitution}}) {
                auto v = JS_GetPropertyStr(c, arg(3), item.first);
                bool has = !JS_IsUndefined(v);
                JS_FreeValue(c, v);
                if (has) {
                    override = &material;
                    if (!Number(c, arg(3), item.first, *item.second))
                        return JS_ThrowTypeError(c, "finite physical factor required");
                }
            }
            std::string error;
            return world.SetBodyMaterial(id, chosen, override, error)
                       ? JS_TRUE
                       : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        auto o = JS_NewObject(c);
        JS_SetPropertyStr(c, o, "asset", asset.empty() ? JS_NULL : JS_NewString(c, asset.c_str()));
        JS_SetPropertyStr(c, o, "friction", JS_NewFloat64(c, friction));
        JS_SetPropertyStr(c, o, "restitution", JS_NewFloat64(c, restitution));
        return o;
    }

    if (op == "sleeping")
        return JS_NewBool(c, world.Physics().IsSleeping(world.RuntimeBody(id)));

    if (op == "body2DInfo") {
        Shape shape;
        BodyTransform pose;
        const auto body = world.RuntimeBody(id);
        if (!world.Physics().GetBodyShape(body, shape, pose) || shape.type != ShapeType::Planar)
            return JS_NULL;
        auto value = PlanarValue(c, *shape.planar);
        JS_SetPropertyStr(c, value, "normal", Vec(c, world.Physics().GetPlanarNormal(body)));
        return value;
    }

    if (op == "colliderInfo") {
        const auto handle = world.RuntimeBody(id);
        Shape shape;
        BodyTransform pose;
        if (!world.Physics().GetBodyShape(handle, shape, pose))
            return JS_NULL;
        auto metadata = [&](const Shape &value, glm::vec3 center, glm::quat rotation,
                            uint32_t key) {
            auto o = JS_NewObject(c);
            const char *type = value.type == ShapeType::Planar          ? "planar"
                               : value.type == ShapeType::Sphere        ? "sphere"
                               : value.type == ShapeType::Box           ? "box"
                               : value.type == ShapeType::ConvexHull    ? "hull"
                               : value.type == ShapeType::TriangleMesh  ? "triangle-mesh"
                               : value.type == ShapeType::CompoundBoxes ? "compound"
                               : value.type == ShapeType::Capsule       ? "capsule"
                                                                        : "terrain";
            JS_SetPropertyStr(c, o, "type", JS_NewString(c, type));
            JS_SetPropertyStr(c, o, "key", JS_NewUint32(c, key));
            JS_SetPropertyStr(c, o, "position", Vec(c, center));
            auto q = Vec(c, {rotation.x, rotation.y, rotation.z});
            JS_SetPropertyStr(c, q, "w", JS_NewFloat64(c, rotation.w));
            JS_SetPropertyStr(c, o, "rotation", q);
            JS_SetPropertyStr(c, o, "halfExtents",
                              value.type == ShapeType::Box ? Vec(c, value.halfExtents) : JS_NULL);
            JS_SetPropertyStr(c, o, "radius",
                              (value.type == ShapeType::Sphere || value.type == ShapeType::Capsule)
                                  ? JS_NewFloat64(c, value.radius)
                                  : JS_NULL);
            JS_SetPropertyStr(c, o, "halfHeight",
                              value.type == ShapeType::Capsule ? JS_NewFloat64(c, value.halfHeight)
                                                               : JS_NULL);
            JS_SetPropertyStr(c, o, "asset",
                              value.asset ? JS_NewString(c, value.assetId.c_str()) : JS_NULL);
            JS_SetPropertyStr(c, o, "vertexCount",
                              JS_NewUint32(c, value.asset ? value.asset->vertices.size() : 0));
            JS_SetPropertyStr(c, o, "triangleCount",
                              JS_NewUint32(c, value.asset ? value.asset->faces.size() : 0));
            JS_SetPropertyStr(c, o, "twoSided",
                              JS_NewBool(c, value.asset && value.asset->twoSided));
            return o;
        };
        auto result = metadata(shape, {0, 0, 0}, {1, 0, 0, 0}, 0);
        JS_SetPropertyStr(c, result, "centerOfMassOffset", Vec(c, shape.pivotOffset));
        JS_SetPropertyStr(c, result, "enabled",
                          JS_NewBool(c, world.Physics().IsBodyEnabled(handle)));
        JS_SetPropertyStr(c, result, "sensor", JS_NewBool(c, world.Physics().IsBodySensor(handle)));
        auto children = JS_NewArray(c);
        for (unsigned i = 0; i < shape.boxes.size(); ++i) {
            const auto &child = shape.boxes[i];
            Shape primitive = child.type == ShapeType::Sphere ? Shape::Sphere(child.radius)
                              : child.type == ShapeType::ConvexHull
                                  ? Shape::Cooked(child.asset, child.assetId)
                                  : Shape::Box(child.halfExtents);
            JS_SetPropertyUint32(c, children, i,
                                 metadata(primitive, child.localCenter, child.rotation,
                                          child.key ? child.key : i + 1));
        }
        JS_SetPropertyStr(c, result, "children", children);
        return result;
    }

    if (op == "colliderEnabled") {
        if (!JS_IsBool(arg(2)))
            return JS_ThrowTypeError(c, "boolean required");
        return JS_NewBool(c, world.SetColliderEnabled(id, JS_ToBool(c, arg(2))));
    }

    if (op == "collisionFilter") {
        if (s->inPresentation || world.NetworkClient())
            return JS_ThrowTypeError(c, "collision filters require authoritative gameplay");
        auto options = arg(2);
        if (!KnownOptions(c, options, {"layer", "mask"}))
            return JS_ThrowTypeError(c, "unknown collision filter setting");
        unsigned layer = 0;
        CategoryMask mask = 0;
        if (!world.Physics().GetCollisionFilter(world.RuntimeBody(id), layer, mask))
            return JS_FALSE;
        const auto &registry = world.Categories().collision;
        auto name = JS_GetPropertyStr(c, options, "layer");
        const bool hasLayer = !JS_IsUndefined(name);
        const int selected = JS_IsString(name) ? registry.Find(String(c, name)) : -1;
        JS_FreeValue(c, name);
        if (hasLayer) {
            if (selected < 0)
                return JS_ThrowTypeError(c, "unknown collision layer");
            layer = unsigned(selected);
        }
        auto list = JS_GetPropertyStr(c, options, "mask");
        const bool valid = JS_IsUndefined(list) || namedLayerMask(list, registry, mask);
        JS_FreeValue(c, list);
        if (!valid)
            return JS_ThrowTypeError(
                c, "mask must contain registered collision layer names or be null");
        return JS_NewBool(c, world.SetRuntimeCollisionFilter(id, layer, mask));
    }

    auto body = world.RuntimeBody(id);
    if (op == "motionType") {
        if (!body.IsValid())
            return JS_NULL;
        const auto mode = world.Physics().GetMotionType(body);
        return JS_NewString(c, mode == BodyMotionType::Static    ? "static"
                               : mode == BodyMotionType::Dynamic ? "dynamic"
                                                                 : "kinematic");
    }

    if (op == "motionTypeSet") {
        if (s->inPresentation)
            return JS_ThrowTypeError(c, "body authority cannot change during presentationUpdate");
        const auto type = String(c, arg(2));
        SceneBodyMotion mode;
        if (type == "static")
            mode = SceneBodyMotion::Static;
        else if (type == "dynamic")
            mode = SceneBodyMotion::Dynamic;
        else if (type == "kinematic")
            mode = SceneBodyMotion::Kinematic;
        else
            return JS_ThrowTypeError(c, "motion type must be static, dynamic or kinematic");
        auto options = arg(3);
        if (!KnownOptions(c, options, {"preserveVelocity"}))
            return JS_ThrowTypeError(c, "authority options accept preserveVelocity only");
        auto preserve = JS_GetPropertyStr(c, options, "preserveVelocity");
        const bool valid = JS_IsUndefined(preserve) || JS_IsBool(preserve);
        const bool retain = JS_IsBool(preserve) && JS_ToBool(c, preserve);
        JS_FreeValue(c, preserve);
        if (!valid)
            return JS_ThrowTypeError(c, "preserveVelocity must be boolean");
        std::string error;
        if (!world.SetRuntimeMotionType(id, mode, retain, error))
            return JS_ThrowTypeError(c, "authority change: %s", error.c_str());
        return JS_TRUE;
    }

    if (op == "pointVelocity") {
        glm::vec3 point;
        if (!body.IsValid() || !ReadVec(c, arg(2), point))
            return JS_ThrowTypeError(c, "live body and finite world point required");
        return Vec(c, world.Physics().GetPointVelocity(body, point));
    }

    if (op == "kinematicMotion") {
        KinematicMotionState state;
        if (!world.Physics().GetKinematicMotion(body, state))
            return JS_NULL;
        auto result = JS_NewObject(c);
        JS_SetPropertyStr(c, result, "control",
                          JS_NewString(c, state.control == KinematicControl::Target ? "target"
                                          : state.control == KinematicControl::Velocity
                                              ? "velocity"
                                              : "stopped"));
        auto target = JS_NewObject(c);
        JS_SetPropertyStr(c, target, "position", Vec(c, state.target.position));
        auto q =
            Vec(c, {state.target.rotation.x, state.target.rotation.y, state.target.rotation.z});
        JS_SetPropertyStr(c, q, "w", JS_NewFloat64(c, state.target.rotation.w));
        JS_SetPropertyStr(c, target, "rotation", q);
        JS_SetPropertyStr(c, result, "target", target);
        JS_SetPropertyStr(c, result, "linearVelocity", Vec(c, state.linearVelocity));
        JS_SetPropertyStr(c, result, "angularVelocity", Vec(c, state.angularVelocity));
        JS_SetPropertyStr(c, result, "remainingSeconds", JS_NewFloat64(c, state.remainingSeconds));
        JS_SetPropertyStr(c, result, "targetNextStep", JS_NewBool(c, state.targetNextStep));
        return result;
    }

    if (op == "kinematicTarget" || op == "kinematicVelocity" || op == "kinematicStop") {
        if (s->inPresentation)
            return JS_ThrowTypeError(
                c, "kinematic commands cannot be published during presentationUpdate");
        if (!world.Physics().IsKinematicBody(body))
            return JS_ThrowTypeError(c, "entity has no active kinematic body");
        bool accepted = false;
        if (op == "kinematicTarget") {
            if (!KnownOptions(c, arg(2), {"position", "rotation"}))
                return JS_ThrowTypeError(c, "kinematic target requires position/rotation only");
            SceneTransform target;
            auto p = JS_GetPropertyStr(c, arg(2), "position"),
                 q = JS_GetPropertyStr(c, arg(2), "rotation");
            const bool complete = !JS_IsUndefined(p) && !JS_IsUndefined(q);
            JS_FreeValue(c, p);
            JS_FreeValue(c, q);
            double seconds = 0;
            if (!complete || !readTransform(arg(2), target) || JS_ToFloat64(c, &seconds, arg(3)) ||
                !std::isfinite(seconds) || seconds < 0 || seconds > 60)
                return JS_ThrowTypeError(c,
                                         "complete finite target and seconds in [0,60] required");
            accepted = world.Physics().MoveKinematic(
                body, {target.position, glm::normalize(target.rotation)}, float(seconds));
        } else if (op == "kinematicVelocity") {
            glm::vec3 linear, angular;
            if (!ReadVec(c, arg(2), linear) || !ReadVec(c, arg(3), angular))
                return JS_ThrowTypeError(c,
                                         "finite world COM m/s and world angular rad/s required");
            accepted = world.Physics().SetKinematicVelocity(body, linear, angular);
        } else
            accepted = world.Physics().StopKinematic(body);
        if (!accepted)
            return JS_ThrowTypeError(c, "unsupported or unusable kinematic command");
        return JS_TRUE;
    }

    if ((op == "velocity" || op == "angularVelocity") && body.IsValid() &&
        (world.Physics().IsDynamicBody(body) || world.Physics().IsKinematicBody(body)))
        return Vec(c, op == "velocity" ? world.Physics().GetLinearVelocity(body)
                                       : world.Physics().GetAngularVelocity(body));

    if (!body.IsValid() || !world.Physics().IsDynamicBody(body))
        return JS_ThrowTypeError(c, "entity has no active dynamic body");

    if (op == "mass")
        return JS_NewFloat64(c, world.Physics().GetMass(body));

    if (op == "centerOfMass")
        return Vec(c, world.Physics().GetCenterOfMass(body));

    if (op == "inertiaWorld") {
        const auto matrix = world.Physics().GetInertiaWorld(body);
        auto result = JS_NewObject(c);
        JS_SetPropertyStr(c, result, "x", Vec(c, matrix[0]));
        JS_SetPropertyStr(c, result, "y", Vec(c, matrix[1]));
        JS_SetPropertyStr(c, result, "z", Vec(c, matrix[2]));
        return result;
    }

    if (op == "velocity")
        return Vec(c, world.Physics().GetLinearVelocity(body));

    if (op == "angularVelocity")
        return Vec(c, world.Physics().GetAngularVelocity(body));

    glm::vec3 v;

    if (!ReadVec(c, arg(2), v))
        return JS_ThrowTypeError(c, "invalid vector");

    if (op == "force")
        world.Physics().ApplyForce(body, v);

    else if (op == "forceAtPoint") {
        glm::vec3 point;
        if (!ReadVec(c, arg(3), point))
            return JS_ThrowTypeError(c, "invalid force point");
        const auto torque = glm::cross(point - world.Physics().GetCenterOfMass(body), v);
        if (!std::isfinite(torque.x) || !std::isfinite(torque.y) || !std::isfinite(torque.z))
            return JS_ThrowRangeError(c, "force point produces non-finite torque");
        world.Physics().ApplyForceAtPoint(body, v, point);
    } else if (op == "impulse")
        world.Physics().ApplyLinearImpulse(body, v);

    else if (op == "impulseAtPoint") {
        glm::vec3 point;
        if (!ReadVec(c, arg(3), point))
            return JS_ThrowTypeError(c, "invalid impulse point");
        world.Physics().ApplyImpulseAtPoint(body, v, point);
    } else if (op == "torque")
        world.Physics().ApplyTorque(body, v);

    else if (op == "setVelocity" || op == "setAngularVelocity") {
        try {
            if (op == "setVelocity")
                world.Physics().SetLinearVelocity(body, v);
            else
                world.Physics().SetAngularVelocity(body, v);
        } catch (const std::invalid_argument &error) {
            return JS_ThrowTypeError(c, "%s", error.what());
        }
    } else
        return JS_ThrowTypeError(c, "unknown engine operation");

    return JS_UNDEFINED;
}
} // namespace
void RegisterPhysicsBodiesBindings(BindingRegistry &registry) {
    registry.Add("PhysicsBodies",
                 {"entityGravityState",
                  "entityGravitySelect",
                  "entityGravityUniform",
                  "entityGravityClear",
                  "physicalMaterialInfo",
                  "physicalMaterialSet",
                  "sleeping",
                  "body2DInfo",
                  "colliderInfo",
                  "colliderEnabled",
                  "collisionFilter",
                  "motionType",
                  "motionTypeSet",
                  "pointVelocity",
                  "kinematicMotion",
                  "kinematicTarget",
                  "kinematicVelocity",
                  "kinematicStop",
                  "velocity",
                  "angularVelocity",
                  "mass",
                  "centerOfMass",
                  "inertiaWorld",
                  "force",
                  "forceAtPoint",
                  "impulse",
                  "impulseAtPoint",
                  "torque",
                  "setVelocity",
                  "setAngularVelocity"},
                 HandlePhysicsBodies);
}
} // namespace ScriptBindings
