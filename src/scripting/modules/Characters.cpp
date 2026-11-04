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

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandleCharacters(BindingContext &context, const std::string &op, int argc,
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

    if (op.rfind("character", 0) == 0) {
        uint64_t id = 0;
        try {
            id = std::stoull(String(c, arg(1)));
        } catch (...) {
            return JS_ThrowReferenceError(c, "invalid character entity");
        }
        auto *m = world.RuntimeCharacter(id);
        if (op == "characterExists")
            return JS_NewBool(c, m != nullptr);
        if (!m)
            return JS_ThrowReferenceError(c, "stale entity or missing character motor");
        if (op == "characterVelocity" || op == "characterAcceleration" ||
            op == "characterDisplacement") {
            if (!s->fixed)
                return JS_ThrowTypeError(c, "character intent must be supplied in fixedUpdate");
            if (op == "characterDisplacement" && !s->animationControl)
                return JS_ThrowTypeError(
                    c, "displacement belongs in fixedUpdate, before motor resolution");
            glm::vec3 v;
            if (!ReadVec(c, arg(2), v))
                return JS_ThrowTypeError(c, "finite vector required");
            if (op == "characterVelocity")
                m->velocity = v;
            else if (op == "characterDisplacement")
                m->displacementIntent += v;
            else
                m->acceleration += v;
            return JS_TRUE;
        }
        if (op == "characterOrientation") {
            if (!s->fixed || !s->animationControl)
                return JS_ThrowTypeError(c, "character orientation belongs in fixedUpdate");
            glm::vec3 xyz;
            float w = 0;
            if (!ReadVec(c, arg(2), xyz) || !Number(c, arg(2), "w", w))
                return JS_ThrowTypeError(c, "finite quaternion required");
            glm::quat q(w, xyz);
            if (!std::isfinite(glm::dot(q, q)) || glm::dot(q, q) < 1e-12f)
                return JS_ThrowTypeError(c, "nondegenerate quaternion required");
            m->orientationIntent = glm::normalize(q);
            return JS_TRUE;
        }
        if (op == "characterEnabled" || op == "characterConfigure") {
            auto config = m->settings;
            if (op == "characterEnabled") {
                if (!JS_IsBool(arg(2)))
                    return JS_ThrowTypeError(c, "boolean required");
                config.enabled = JS_ToBool(c, arg(2));
            } else {
                const char *keys[] = {"radius",
                                      "halfHeight",
                                      "stepHeight",
                                      "supportDistance",
                                      "skin",
                                      "maxSlopeDegrees",
                                      "gravityScale",
                                      "reorientationDegreesPerSecond",
                                      "interactionMass",
                                      "maxPushImpulse"};
                float *values[] = {&config.radius,
                                   &config.halfHeight,
                                   &config.stepHeight,
                                   &config.supportDistance,
                                   &config.skin,
                                   &config.maxSlopeDegrees,
                                   &config.gravityScale,
                                   &config.reorientationDegreesPerSecond,
                                   &config.interactionMass,
                                   &config.maxPushImpulse};
                for (size_t i = 0; i < 10; ++i) {
                    auto v = JS_GetPropertyStr(c, arg(2), keys[i]);
                    bool exists = !JS_IsUndefined(v);
                    JS_FreeValue(c, v);
                    if (exists && !Number(c, arg(2), keys[i], *values[i]))
                        return JS_ThrowTypeError(c, "invalid motor setting");
                }
            }
            if (op == "characterConfigure") {
                auto offset = JS_GetPropertyStr(c, arg(2), "offset");
                bool ok = JS_IsUndefined(offset) || ReadVec(c, offset, config.offset);
                JS_FreeValue(c, offset);
                if (!ok)
                    return JS_ThrowTypeError(c, "invalid offset");
                auto layer = JS_GetPropertyStr(c, arg(2), "collisionLayer");
                if (!JS_IsUndefined(layer)) {
                    int found = world.Categories().collision.Find(String(c, layer));
                    JS_FreeValue(c, layer);
                    if (found < 0)
                        return JS_ThrowTypeError(c, "unknown collision layer");
                    config.collisionLayer = unsigned(found);
                } else
                    JS_FreeValue(c, layer);
                for (const auto &entry : std::vector<std::pair<const char *, CategoryMask *>>{
                         {"requiredTags", &config.requiredTags},
                         {"excludedTags", &config.excludedTags}}) {
                    auto a = JS_GetPropertyStr(c, arg(2), entry.first);
                    bool valid = JS_IsUndefined(a) || tagMask(a, *entry.second);
                    JS_FreeValue(c, a);
                    if (!valid)
                        return JS_ThrowTypeError(c, "unknown motor tag");
                }
                auto mask = JS_GetPropertyStr(c, arg(2), "collisionMask");
                if (!JS_IsUndefined(mask)) {
                    uint32_t count = 0;
                    auto n = JS_GetPropertyStr(c, mask, "length");
                    bool valid = JS_IsArray(mask) && JS_ToUint32(c, &count, n) == 0 && count <= 64;
                    JS_FreeValue(c, n);
                    CategoryMask bits = 0;
                    for (uint32_t i = 0; valid && i < count; ++i) {
                        auto v = JS_GetPropertyUint32(c, mask, i);
                        int found = world.Categories().collision.Find(String(c, v));
                        JS_FreeValue(c, v);
                        if (found < 0)
                            valid = false;
                        else
                            bits |= CategoryBit(found);
                    }
                    JS_FreeValue(c, mask);
                    if (!valid)
                        return JS_ThrowTypeError(c, "unknown motor layer mask");
                    config.collisionMask = bits;
                } else
                    JS_FreeValue(c, mask);
            }
            std::string error;
            if (!ValidCharacterMotor(config, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            world.SetCharacterSettings(id, config);
            return JS_TRUE;
        }
        if (op == "characterIgnore") {
            if (!JS_IsArray(arg(2)))
                return JS_ThrowTypeError(c, "entity array required");
            auto n = JS_GetPropertyStr(c, arg(2), "length");
            uint32_t length = 0;
            JS_ToUint32(c, &length, n);
            JS_FreeValue(c, n);
            if (length > 64)
                return JS_ThrowTypeError(c, "too many ignored entities");
            std::vector<BodyHandle> handles;
            for (uint32_t i = 0; i < length; ++i) {
                auto v = JS_GetPropertyUint32(c, arg(2), i);
                uint64_t other = 0;
                try {
                    other = std::stoull(String(c, v));
                } catch (...) {
                    JS_FreeValue(c, v);
                    return JS_ThrowTypeError(c, "invalid ignored entity");
                }
                JS_FreeValue(c, v);
                auto body = world.RuntimeBody(other);
                if (body.IsValid())
                    handles.push_back(body);
            }
            m->filter.ignoredBodies = std::move(handles);
            return JS_TRUE;
        }
        if (op != "characterState")
            return JS_ThrowTypeError(c, "unknown character operation");
        auto o = JS_NewObject(c);
        const auto &r = m->result;
        JS_SetPropertyStr(c, o, "velocity", Vec(c, m->velocity));
        JS_SetPropertyStr(c, o, "actualDisplacement", Vec(c, r.displacement));
        JS_SetPropertyStr(c, o, "supportNormal", Vec(c, r.supportNormal));
        JS_SetPropertyStr(c, o, "supportVelocity", Vec(c, r.supportVelocity));
        JS_SetPropertyStr(c, o, "gravity", Vec(c, world.SampleEntityGravity(id, m->position)));
        JS_SetPropertyStr(c, o, "up", Vec(c, m->orientation * glm::vec3(0, 1, 0)));
        JS_SetPropertyStr(c, o, "supported", JS_NewBool(c, r.supported));
        JS_SetPropertyStr(c, o, "collided", JS_NewBool(c, r.collided));
        JS_SetPropertyStr(c, o, "supportEntityId",
                          JS_NewString(c, std::to_string(world.EntityIdOfBody(r.support)).c_str()));
        return o;
    }
    return JS_ThrowTypeError(c, "unhandled Characters operation");
}
} // namespace
void RegisterCharactersBindings(BindingRegistry &registry) {
    registry.Add("Characters",
                 {"characterExists", "characterVelocity", "characterAcceleration",
                  "characterDisplacement", "characterOrientation", "characterEnabled",
                  "characterConfigure", "characterIgnore", "characterState"},
                 HandleCharacters);
}
} // namespace ScriptBindings
