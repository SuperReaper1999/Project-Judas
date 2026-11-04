#include "../../PerformanceProfiler.h"
#include "../../PlayerController.h"
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
JSValue HandlePhysicsQueries(BindingContext &context, const std::string &op, int argc,
                             JSValueConst *argv) {
    auto *s = &context;
    auto *c = context.js;
    auto &world = *s->world;
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };
    auto tagMask = [&](JSValueConst array, CategoryMask &mask) {
        if (!JS_IsArray(array))
            return false;
        mask = 0;
        auto length = JS_GetPropertyStr(c, array, "length");
        uint32_t n = 0;
        JS_ToUint32(c, &n, length);
        JS_FreeValue(c, length);
        if (n > 64)
            return false;
        for (uint32_t i = 0; i < n; ++i) {
            auto item = JS_GetPropertyUint32(c, array, i);
            auto name = String(c, item);
            JS_FreeValue(c, item);
            int id = world.Categories().tags.Find(name);
            if (id < 0)
                return false;
            mask |= CategoryBit(id);
        }
        return true;
    };

    auto ids = [&](const std::vector<EntityId> &values) {
        auto array = JS_NewArray(c);
        uint32_t i = 0;
        for (auto id : values)
            JS_SetPropertyUint32(c, array, i++, JS_NewString(c, std::to_string(id).c_str()));
        return array;
    };

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

    auto queryFilter = [&](JSValueConst options, PhysicsQueryFilter &filter, std::string &error) {
        JUDAS_PROFILE_COUNTER("Physics query filter preparations", 1, ProfileCounterMode::Sum);
        auto include = JS_GetPropertyStr(c, options, "includeLayers");
        auto exclude = JS_GetPropertyStr(c, options, "excludeLayers");
        auto layerMask = [&](JSValueConst a, CategoryMask &mask) {
            if (JS_IsUndefined(a))
                return true;
            if (!JS_IsArray(a))
                return false;
            auto len = JS_GetPropertyStr(c, a, "length");
            uint32_t n = 0;
            JS_ToUint32(c, &n, len);
            JS_FreeValue(c, len);
            if (n > 64)
                return false;
            mask = 0;
            for (uint32_t i = 0; i < n; ++i) {
                auto v = JS_GetPropertyUint32(c, a, i);
                auto name = String(c, v);
                JS_FreeValue(c, v);
                int id = world.Categories().collision.Find(name);
                if (id < 0)
                    return false;
                mask |= CategoryBit(id);
            }
            return true;
        };
        auto sensors = JS_GetPropertyStr(c, options, "includeSensors");
        if (!JS_IsUndefined(sensors) && !JS_IsBool(sensors)) {
            JS_FreeValue(c, sensors);
            JS_FreeValue(c, include);
            JS_FreeValue(c, exclude);
            error = "includeSensors must be boolean";
            return false;
        }
        filter.includeSensors = JS_ToBool(c, sensors) > 0;
        JS_FreeValue(c, sensors);
        auto required = JS_GetPropertyStr(c, options, "requiredTags"),
             excluded = JS_GetPropertyStr(c, options, "excludedTags"),
             ignored = JS_GetPropertyStr(c, options, "ignored");
        bool tagsOk = (JS_IsUndefined(required) || tagMask(required, filter.requiredTags)) &&
                      (JS_IsUndefined(excluded) || tagMask(excluded, filter.excludedTags));
        JS_FreeValue(c, required);
        JS_FreeValue(c, excluded);
        if (!JS_IsUndefined(ignored)) {
            auto len = JS_GetPropertyStr(c, ignored, "length");
            uint32_t n = 0;
            JS_ToUint32(c, &n, len);
            JS_FreeValue(c, len);
            if (n > 256)
                tagsOk = false;
            for (uint32_t i = 0; tagsOk && i < n; ++i) {
                auto v = JS_GetPropertyUint32(c, ignored, i);
                auto idv = JS_GetPropertyStr(c, v, "id");
                try {
                    auto body = world.RuntimeBody(std::stoull(String(c, idv)));
                    if (body.IsValid())
                        filter.ignoredBodies.push_back(body);
                } catch (...) {
                    tagsOk = false;
                }
                JS_FreeValue(c, idv);
                JS_FreeValue(c, v);
            }
        }
        JS_FreeValue(c, ignored);
        bool ok = tagsOk && layerMask(include, filter.includeLayers) &&
                  layerMask(exclude, filter.excludeLayers);
        JS_FreeValue(c, include);
        JS_FreeValue(c, exclude);
        if (!ok) {
            error = "unknown collision layer/tag or invalid ignored entity";
            return false;
        }
        return true;
    };

    struct PreparedCast {
        std::string kind;
        BodyTransform pose;
        glm::vec3 direction{0}, halfExtents{0};
        float maximum = 0, radius = 0, halfHeight = 0;
    };

    auto prepareCast = [&](JSValueConst origin, JSValueConst direction, JSValueConst spec,
                           const std::string &kind, JSValueConst rotation, PreparedCast &request,
                           std::string &error, bool readCoordinates = true) {
        request.kind = kind;
        if (readCoordinates && (!ReadVec(c, origin, request.pose.position) ||
                                !ReadVec(c, direction, request.direction))) {
            error = "invalid bounds";
            return false;
        }
        if (!Number(c, spec, "maximum", request.maximum)) {
            error = "invalid cast maximum";
            return false;
        }
        if ((kind == "sphere" || kind == "capsule") && !Number(c, spec, "radius", request.radius)) {
            error = "invalid radius";
            return false;
        }
        if (kind == "capsule" && !Number(c, spec, "halfHeight", request.halfHeight)) {
            error = "invalid capsule halfHeight";
            return false;
        }
        if (kind == "box") {
            auto h = JS_GetPropertyStr(c, spec, "halfExtents");
            bool valid = ReadVec(c, h, request.halfExtents);
            JS_FreeValue(c, h);
            if (!valid) {
                error = "invalid halfExtents";
                return false;
            }
        }
        if (kind == "box" || kind == "capsule")
            if (!JS_IsUndefined(rotation) && !(Number(c, rotation, "w", request.pose.rotation.w) &&
                                               Number(c, rotation, "x", request.pose.rotation.x) &&
                                               Number(c, rotation, "y", request.pose.rotation.y) &&
                                               Number(c, rotation, "z", request.pose.rotation.z))) {
                error = "invalid cast rotation";
                return false;
            }
        if (kind != "ray" && kind != "sphere" && kind != "capsule" && kind != "box") {
            error = "unknown cast shape";
            return false;
        }
        // These are the existing PhysicsWorld::Cast admissibility rules, evaluated
        // before the batch's first query (double norms preserve tiny directions).
        auto q = request.pose.rotation;
        double q2 = glm::dot(glm::dvec4(q.x, q.y, q.z, q.w), glm::dvec4(q.x, q.y, q.z, q.w));
        double d2 = glm::dot(glm::dvec3(request.direction), glm::dvec3(request.direction));
        if (request.maximum < 0 || d2 <= 0 || !std::isfinite(q2) || q2 <= 0) {
            error = "cast requires nonzero orientation/direction and nonnegative distance";
            return false;
        }
        if (request.radius < 0 || request.halfHeight < 0 ||
            (kind == "box" && (request.halfExtents.x <= 0 || request.halfExtents.y <= 0 ||
                               request.halfExtents.z <= 0))) {
            error = "invalid cast dimensions";
            return false;
        }
        return true;
    };

    auto executeCast = [&](const PreparedCast &request, const PhysicsQueryFilter &filter) {
        JUDAS_PROFILE_SCOPE("Physics query geometry");
        JUDAS_PROFILE_COUNTER("Physics geometric queries", 1, ProfileCounterMode::Sum);
        if (request.kind == "ray")
            return world.Physics().Raycast(request.pose.position, request.direction,
                                           request.maximum, filter);
        if (request.kind == "sphere")
            return world.Physics().SphereCast(request.pose.position, request.radius,
                                              request.direction, request.maximum, filter);
        if (request.kind == "capsule")
            return world.Physics().CapsuleCast(request.pose, request.radius, request.halfHeight,
                                               request.direction, request.maximum, filter);
        return world.Physics().BoxCast(request.pose, request.halfExtents, request.direction,
                                       request.maximum, filter);
    };

    auto castHit = [&](const PhysicsCastHit &hit) -> JSValue {
        if (!hit.hit)
            return JS_NULL;
        auto o = JS_NewObject(c);
        auto id = world.EntityIdOfBody(hit.body);
        JS_SetPropertyStr(c, o, "entityId", JS_NewString(c, std::to_string(id).c_str()));
        JS_SetPropertyStr(c, o, "bodyId", JS_NewUint32(c, hit.body.id));
        JS_SetPropertyStr(c, o, "point", Vec(c, hit.point));
        JS_SetPropertyStr(c, o, "normal", Vec(c, hit.normal));
        JS_SetPropertyStr(c, o, "distance", JS_NewFloat64(c, hit.distance));
        JS_SetPropertyStr(c, o, "fraction", JS_NewFloat64(c, hit.fraction));
        std::string physical;
        float friction = 0, restitution = 0;
        world.Physics().GetPhysicalMaterial(hit.body, physical, friction, restitution);
        JS_SetPropertyStr(c, o, "physicalMaterial",
                          physical.empty() ? JS_NULL : JS_NewString(c, physical.c_str()));
        JS_SetPropertyStr(c, o, "primitiveIndex", JS_NewInt32(c, hit.primitiveIndex));
        JS_SetPropertyStr(c, o, "childKey", JS_NewUint32(c, hit.childKey));
        JS_SetPropertyStr(c, o, "feature",
                          hit.feature == UINT32_MAX ? JS_NULL : JS_NewUint32(c, hit.feature));
        JS_SetPropertyStr(c, o, "initialOverlap", JS_NewBool(c, hit.initialOverlap));
        JS_SetPropertyStr(c, o, "shape",
                          JS_NewString(c, hit.shape == ShapeType::Planar         ? "planar"
                                          : hit.shape == ShapeType::Capsule      ? "capsule"
                                          : hit.shape == ShapeType::Sphere       ? "sphere"
                                          : hit.shape == ShapeType::Terrain      ? "terrain"
                                          : hit.shape == ShapeType::ConvexHull   ? "hull"
                                          : hit.shape == ShapeType::TriangleMesh ? "triangle-mesh"
                                                                                 : "box"));
        return o;
    };

    if (op == "gravitySample") {
        glm::vec3 point;
        if (!ReadVec(c, arg(1), point))
            return JS_ThrowTypeError(c, "finite world position required");
        return Vec(c, world.Gravity().Sample(point));
    }

    if (op == "cast2D") {
        std::shared_ptr<const PlanarCollider> shape;
        SceneTransform pose;
        glm::vec3 direction;
        double maximum = 0;
        PhysicsQueryFilter filter;
        std::string error;
        if (!ReadPlanarValues(c, arg(1), shape) || !readTransform(arg(2), pose) ||
            pose.scale != glm::vec3(1) || !ReadVec(c, arg(3), direction) ||
            JS_ToFloat64(c, &maximum, arg(4)) || !std::isfinite(maximum) || maximum < 0 ||
            maximum > 1e6 || !queryFilter(arg(5), filter, error))
            return JS_ThrowTypeError(c, "cast2D: convex native 2D shape, unit-scale pose, finite "
                                        "direction/distance and valid filter required");
        try {
            return castHit(world.Physics().CastPlanar(Shape::Planar(shape),
                                                      {pose.position, pose.rotation}, direction,
                                                      float(maximum), filter));
        } catch (const std::exception &e) {
            return JS_ThrowTypeError(c, "cast2D: %s", e.what());
        }
    }

    if (op == "overlapShape") {
        const auto spec = arg(1);
        const auto placement = arg(2);
        double limit = 0;
        if (!JS_IsNumber(arg(3)) || JS_ToFloat64(c, &limit, arg(3)) || !std::isfinite(limit) ||
            limit < 1 || limit > 256 || limit != std::floor(limit))
            return JS_ThrowRangeError(c, "overlapShape limit must be an integer from 1 to 256");
        auto kindValue = JS_GetPropertyStr(c, spec, "kind");
        const auto kind = String(c, kindValue);
        JS_FreeValue(c, kindValue);
        Shape shape;
        if (kind == "box") {
            auto value = JS_GetPropertyStr(c, spec, "halfExtents");
            glm::vec3 halfExtents;
            const bool valid = ReadVec(c, value, halfExtents) &&
                               glm::all(glm::greaterThan(halfExtents, glm::vec3(0)));
            JS_FreeValue(c, value);
            if (!valid)
                return JS_ThrowTypeError(c, "positive box halfExtents required");
            shape = Shape::Box(halfExtents);
        } else if (kind == "sphere" || kind == "capsule") {
            float radius = 0, halfHeight = 0;
            if (!Number(c, spec, "radius", radius) || radius <= 0 ||
                (kind == "capsule" &&
                 (!Number(c, spec, "halfHeight", halfHeight) || halfHeight < 0)))
                return JS_ThrowTypeError(
                    c, "positive radius and nonnegative capsule halfHeight required");
            shape = kind == "sphere" ? Shape::Sphere(radius) : Shape::Capsule(radius, halfHeight);
        } else
            return JS_ThrowTypeError(c, "overlapShape requires box, sphere or capsule");
        SceneTransform pose;
        if (!KnownOptions(c, placement, {"position", "rotation"}))
            return JS_ThrowTypeError(
                c, "overlapShape requires a finite position and optional rotation");
        auto position = JS_GetPropertyStr(c, placement, "position");
        auto rotation = JS_GetPropertyStr(c, placement, "rotation");
        const bool validPose =
            ReadVec(c, position, pose.position) &&
            (JS_IsUndefined(rotation) || (Number(c, rotation, "w", pose.rotation.w) &&
                                          Number(c, rotation, "x", pose.rotation.x) &&
                                          Number(c, rotation, "y", pose.rotation.y) &&
                                          Number(c, rotation, "z", pose.rotation.z) &&
                                          std::isfinite(glm::dot(pose.rotation, pose.rotation)) &&
                                          glm::dot(pose.rotation, pose.rotation) > 1e-12f));
        JS_FreeValue(c, position);
        JS_FreeValue(c, rotation);
        if (!validPose)
            return JS_ThrowTypeError(
                c, "overlapShape requires a finite position and optional rotation");
        pose.rotation = glm::normalize(pose.rotation);
        PhysicsQueryFilter filter;
        std::string error;
        if (!queryFilter(arg(4), filter, error))
            return JS_ThrowTypeError(c, "overlapShape filter: %s", error.c_str());
        bool truncated = false;
        std::vector<BodyHandle> matches;
        try {
            matches = world.Physics().OverlapShape(shape, {pose.position, pose.rotation}, filter,
                                                   unsigned(limit), truncated);
        } catch (const std::exception &exception) {
            return JS_ThrowTypeError(c, "overlapShape: %s", exception.what());
        }
        auto hits = JS_NewArray(c);
        for (uint32_t i = 0; i < matches.size(); ++i) {
            auto hit = JS_NewObject(c);
            JS_SetPropertyStr(c, hit, "bodyId", JS_NewUint32(c, matches[i].id));
            JS_SetPropertyStr(
                c, hit, "entityId",
                JS_NewString(c, std::to_string(world.EntityIdOfBody(matches[i])).c_str()));
            JS_SetPropertyUint32(c, hits, i, hit);
        }
        auto result = JS_NewObject(c);
        JS_SetPropertyStr(c, result, "hits", hits);
        JS_SetPropertyStr(c, result, "truncated", JS_NewBool(c, truncated));
        return result;
    }

    if (op == "castMany") {
        JUDAS_PROFILE_COUNTER("Physics query bridge calls", 1, ProfileCounterMode::Sum);
        auto kind = String(c, arg(1));
        auto requests = arg(2);
        if (kind != "ray" && kind != "sphere" && kind != "capsule")
            return JS_ThrowTypeError(c, "unknown batch cast shape");
        if (!JS_IsArray(requests))
            return JS_ThrowTypeError(c, "castMany requests must be an array");
        auto length = JS_GetPropertyStr(c, requests, "length");
        uint32_t count = 0;
        bool valid = JS_ToUint32(c, &count, length) == 0;
        JS_FreeValue(c, length);
        if (!valid)
            return JS_ThrowTypeError(c, "invalid castMany length");
        if (count > 256)
            return JS_ThrowRangeError(c, "castMany accepts at most 256 requests");
        PhysicsQueryFilter filter;
        std::string error;
        if (!queryFilter(arg(3), filter, error))
            return JS_ThrowTypeError(c, "castMany filter: %s", error.c_str());
        std::array<PreparedCast, 256> prepared;
        for (uint32_t i = 0; i < count; ++i) {
            auto entry = JS_GetPropertyUint32(c, requests, i);
            if (!JS_IsObject(entry)) {
                JS_FreeValue(c, entry);
                return JS_ThrowTypeError(c, "castMany[%u]: request object required", i);
            }
            auto pose = kind == "capsule" ? JS_GetPropertyStr(c, entry, "pose") : JS_UNDEFINED;
            if (kind == "capsule" && !JS_IsObject(pose)) {
                JS_FreeValue(c, pose);
                JS_FreeValue(c, entry);
                return JS_ThrowTypeError(c, "castMany[%u]: capsule pose required", i);
            }
            auto origin = JS_GetPropertyStr(c, kind == "capsule" ? pose : entry,
                                            kind == "capsule" ? "position" : "origin");
            auto direction = JS_GetPropertyStr(c, entry, "direction");
            auto rotation =
                kind == "capsule" ? JS_GetPropertyStr(c, pose, "rotation") : JS_UNDEFINED;
            valid = prepareCast(origin, direction, entry, kind, rotation, prepared[i], error);
            JS_FreeValue(c, rotation);
            JS_FreeValue(c, direction);
            JS_FreeValue(c, origin);
            JS_FreeValue(c, pose);
            JS_FreeValue(c, entry);
            if (!valid)
                return JS_ThrowTypeError(c, "castMany[%u]: %s", i, error.c_str());
        }
        auto results = JS_NewArray(c);
        try {
            for (uint32_t i = 0; i < count; ++i)
                JS_SetPropertyUint32(c, results, i, castHit(executeCast(prepared[i], filter)));
        } catch (const std::exception &e) {
            JS_FreeValue(c, results);
            return JS_ThrowTypeError(c, "castMany: %s", e.what());
        }
        return results;
    }

    if (op == "overlap" || op == "sweep" || op == "cast" || op == "closestPoint") {
        glm::vec3 min, max;
        if (!ReadVec(c, arg(1), min) || !ReadVec(c, arg(2), max))
            return JS_ThrowTypeError(c, "invalid bounds");
        PhysicsQueryFilter filter;
        std::string error;
        if (!queryFilter(arg(3), filter, error))
            return JS_ThrowTypeError(c, "%s", error.c_str());
        if (op == "closestPoint") {
            float maximum = 0;
            if (!Number(c, arg(4), "maximum", maximum))
                return JS_ThrowTypeError(c, "invalid closestPoint maximum");
            PhysicsClosestPoint hit;
            try {
                hit = world.Physics().ClosestPoint(min, maximum, filter);
            } catch (const std::exception &e) {
                return JS_ThrowTypeError(c, "closestPoint: %s", e.what());
            }
            if (!hit.hit)
                return JS_NULL;
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(
                c, o, "entityId",
                JS_NewString(c, std::to_string(world.EntityIdOfBody(hit.body)).c_str()));
            JS_SetPropertyStr(c, o, "bodyId", JS_NewUint32(c, hit.body.id));
            JS_SetPropertyStr(c, o, "point", Vec(c, hit.point));
            JS_SetPropertyStr(c, o, "normal", hit.normalUnique ? Vec(c, hit.normal) : JS_NULL);
            JS_SetPropertyStr(c, o, "distance", JS_NewFloat64(c, hit.distance));
            JS_SetPropertyStr(c, o, "contains",
                              hit.containmentKnown ? JS_NewBool(c, hit.contains) : JS_NULL);
            JS_SetPropertyStr(c, o, "primitiveIndex", JS_NewInt32(c, hit.primitiveIndex));
            JS_SetPropertyStr(c, o, "childKey", JS_NewUint32(c, hit.childKey));
            JS_SetPropertyStr(c, o, "feature",
                              hit.feature == UINT32_MAX ? JS_NULL : JS_NewUint32(c, hit.feature));
            return o;
        }
        if (op == "cast") {
            JUDAS_PROFILE_COUNTER("Physics query bridge calls", 1, ProfileCounterMode::Sum);
            auto spec = arg(4);
            auto value = JS_GetPropertyStr(c, spec, "kind");
            auto kind = String(c, value);
            JS_FreeValue(c, value);
            auto rotation = JS_GetPropertyStr(c, spec, "rotation");
            PreparedCast request;
            request.pose.position = min;
            request.direction = max;
            bool valid = prepareCast(arg(1), arg(2), spec, kind, rotation, request, error, false);
            JS_FreeValue(c, rotation);
            if (!valid)
                return JS_ThrowTypeError(c, "cast: %s", error.c_str());
            try {
                return castHit(executeCast(request, filter));
            } catch (const std::exception &e) {
                return JS_ThrowTypeError(c, "cast: %s", e.what());
            }
        }
        if (op == "sweep") {
            glm::quat rotation;
            auto r = arg(4);
            if (!Number(c, r, "w", rotation.w) || !Number(c, r, "x", rotation.x) ||
                !Number(c, r, "y", rotation.y) || !Number(c, r, "z", rotation.z) ||
                !std::isfinite(glm::dot(rotation, rotation)) ||
                glm::dot(rotation, rotation) < 1e-12f)
                return JS_ThrowTypeError(c, "invalid rotation");
            // Same player-sized capsule as the legacy controller, without requiring its spawned
            // shape: script-owned (non-legacy) worlds never register one, and every sweep reported
            // a miss.
            auto hit = world.Physics().SweepCapsuleMotion(
                PlayerController::CapsuleRadius(), PlayerController::CapsuleHalfHeight(), min,
                glm::normalize(rotation), max, false, 0, 1, &filter, 0, kAllCategories, false);
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "hit", JS_NewBool(c, hit.hit));
            JS_SetPropertyStr(c, o, "distance", JS_NewFloat64(c, hit.distance));
            JS_SetPropertyStr(c, o, "normal", Vec(c, hit.normal));
            auto entityId = world.EntityIdOfBody(hit.hitBody);
            JS_SetPropertyStr(c, o, "entityId", JS_NewString(c, std::to_string(entityId).c_str()));
            return o;
        }
        std::vector<EntityId> result;
        for (auto handle : world.Physics().QueryBodiesInAabb(min, max, filter)) {
            auto id = world.EntityIdOfBody(handle);
            if (!id)
                for (const auto &o : world.ScriptObjects())
                    if (world.RuntimeBody(o.id).id == handle.id) {
                        id = o.id;
                        break;
                    }
            if (id)
                result.push_back(id);
        }
        return ids(result);
    }
    return JS_ThrowTypeError(c, "unhandled PhysicsQueries operation");
}
} // namespace
void RegisterPhysicsQueriesBindings(BindingRegistry &registry) {
    registry.Add("PhysicsQueries",
                 {"gravitySample", "cast2D", "overlapShape", "castMany", "overlap", "sweep", "cast",
                  "closestPoint"},
                 HandlePhysicsQueries);
}
} // namespace ScriptBindings
