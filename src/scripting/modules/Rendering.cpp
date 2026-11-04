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

#include "../../CameraProjection.h"
#include "../../PowertrainAuthoring.h"
#include "../../WorldStreaming.h"

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandleRendering(BindingContext &context, const std::string &op, int argc,
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

    if (op == "appearanceReset")
        return JS_NewBool(c, world.ResetAppearance());

    if (op == "appearanceInfo" || op == "appearanceSet") {
        auto appearance = world.Settings();
        auto options = arg(1);
        auto has = [&](const char *name) {
            auto v = JS_GetPropertyStr(c, options, name);
            bool found = !JS_IsUndefined(v);
            JS_FreeValue(c, v);
            return found;
        };
        if (op == "appearanceSet") {
            for (auto pair : {std::pair<const char *, glm::vec3 *>{"backgroundColor",
                                                                   &appearance.backgroundColor},
                              {"sunDirection", &appearance.sunDirection},
                              {"sunColor", &appearance.sunColor},
                              {"ambientColor", &appearance.ambientColor}})
                if (has(pair.first)) {
                    auto v = JS_GetPropertyStr(c, options, pair.first);
                    bool ok = ReadVec(c, v, *pair.second);
                    JS_FreeValue(c, v);
                    if (!ok)
                        return JS_ThrowTypeError(c, "finite appearance vector required");
                }
            for (auto pair : {std::pair<const char *, float *>{"exposure", &appearance.exposure},
                              {"environmentIntensity", &appearance.environmentIntensity},
                              {"sunIntensity", &appearance.sunIntensity}})
                if (has(pair.first) && !Number(c, options, pair.first, *pair.second))
                    return JS_ThrowTypeError(c, "finite appearance parameter required");
            for (auto pair :
                 {std::pair<const char *, bool *>{"linearRendering", &appearance.linearRendering},
                  {"environmentBackground", &appearance.environmentBackground},
                  {"sunEnabled", &appearance.sunEnabled}})
                if (has(pair.first)) {
                    auto v = JS_GetPropertyStr(c, options, pair.first);
                    if (!JS_IsBool(v)) {
                        JS_FreeValue(c, v);
                        return JS_ThrowTypeError(c, "boolean appearance setting required");
                    }
                    *pair.second = JS_ToBool(c, v);
                    JS_FreeValue(c, v);
                }
            if (has("environmentAsset")) {
                auto v = JS_GetPropertyStr(c, options, "environmentAsset");
                if (!JS_IsString(v)) {
                    JS_FreeValue(c, v);
                    return JS_ThrowTypeError(c, "environment asset ID required");
                }
                appearance.environmentAsset = String(c, v);
                JS_FreeValue(c, v);
            }
            if (has("environmentRotation")) {
                auto v = JS_GetPropertyStr(c, options, "environmentRotation");
                bool ok = Number(c, v, "w", appearance.environmentRotation.w) &&
                          Number(c, v, "x", appearance.environmentRotation.x) &&
                          Number(c, v, "y", appearance.environmentRotation.y) &&
                          Number(c, v, "z", appearance.environmentRotation.z);
                JS_FreeValue(c, v);
                if (!ok)
                    return JS_ThrowTypeError(c, "finite environment quaternion required");
            }
            if (!world.SetAppearance(appearance))
                return JS_ThrowTypeError(c, "invalid appearance/environment asset");
            return JS_TRUE;
        }
        auto result = JS_NewObject(c);
        JS_SetPropertyStr(c, result, "sunEnabled", JS_NewBool(c, appearance.sunEnabled));
        JS_SetPropertyStr(c, result, "sunIntensity", JS_NewFloat64(c, appearance.sunIntensity));
        JS_SetPropertyStr(c, result, "sunDirection", Vec(c, appearance.sunDirection));
        JS_SetPropertyStr(c, result, "sunColor", Vec(c, appearance.sunColor));
        JS_SetPropertyStr(c, result, "ambientColor", Vec(c, appearance.ambientColor));
        auto *resources = world.Resources();
        const char *status =
            appearance.environmentAsset.empty() ? "ready"
            : resources ? ResourceStateName(resources->StateOf(appearance.environmentAsset))
                        : "unloaded";
        JS_SetPropertyStr(c, result, "environmentStatus", JS_NewString(c, status));
        JS_SetPropertyStr(
            c, result, "environmentError",
            JS_NewString(c, resources && !appearance.environmentAsset.empty()
                                ? resources->ErrorOf(appearance.environmentAsset).c_str()
                                : ""));
        JS_SetPropertyStr(c, result, "linearRendering", JS_NewBool(c, appearance.linearRendering));
        JS_SetPropertyStr(c, result, "backgroundColor", Vec(c, appearance.backgroundColor));
        JS_SetPropertyStr(c, result, "exposure", JS_NewFloat64(c, appearance.exposure));
        JS_SetPropertyStr(c, result, "environmentAsset",
                          JS_NewString(c, appearance.environmentAsset.c_str()));
        JS_SetPropertyStr(c, result, "environmentIntensity",
                          JS_NewFloat64(c, appearance.environmentIntensity));
        JS_SetPropertyStr(c, result, "environmentBackground",
                          JS_NewBool(c, appearance.environmentBackground));
        auto q = Vec(c, {appearance.environmentRotation.x, appearance.environmentRotation.y,
                         appearance.environmentRotation.z});
        JS_SetPropertyStr(c, q, "w", JS_NewFloat64(c, appearance.environmentRotation.w));
        JS_SetPropertyStr(c, result, "environmentRotation", q);
        return result;
    }

    if (op == "setView") {
        const bool initialView = !world.view;
        SceneTransform t;
        double fov = 70;
        float nearPlane = .1f, farPlane = 500;
        auto range = arg(3);
        CameraLens lens;
        if (!KnownOptions(c, range,
                          {"near", "far", "projection", "height", "pixelPerfect", "referenceWidth",
                           "referenceHeight", "pixelsPerUnit"}) ||
            !ReadCameraLens(c, range, lens))
            return JS_ThrowTypeError(c,
                                     "invalid camera projection, pixel settings or unknown option");
        for (auto [key, value] :
             {std::pair<const char *, float *>{"near", &nearPlane}, {"far", &farPlane}}) {
            auto v = JS_GetPropertyStr(c, range, key);
            bool has = !JS_IsUndefined(v);
            JS_FreeValue(c, v);
            if (has && !Number(c, range, key, *value))
                return JS_ThrowTypeError(c, "finite camera range required");
        }
        if (!readTransform(arg(1), t) || JS_ToFloat64(c, &fov, arg(2)) ||
            !world.SetRuntimeView(t, float(fov), nearPlane, farPlane, lens))
            return JS_ThrowTypeError(c, "invalid runtime view");
        if (initialView)
            world.ResetAudioMotion();
        return JS_TRUE;
    }

    if (op == "viewportSize") {
        auto o = JS_NewObject(c);
        JS_SetPropertyStr(c, o, "width", JS_NewInt32(c, world.viewportWidth));
        JS_SetPropertyStr(c, o, "height", JS_NewInt32(c, world.viewportHeight));
        return o;
    }

    if (op == "projectViewport" || op == "viewportRay") {
        if (!world.view || world.viewportWidth <= 0 || world.viewportHeight <= 0)
            return JS_NULL;
        const auto &v = *world.view;
        const auto frame =
            BuildCameraFrame(v.pose.position, v.pose.rotation, v.fov, v.nearPlane, v.farPlane,
                             v.lens, world.viewportWidth, world.viewportHeight);
        if (op == "viewportRay") {
            glm::vec2 point;
            glm::vec3 origin, direction;
            if (!Number(c, arg(1), "x", point.x) || !Number(c, arg(1), "y", point.y))
                return JS_ThrowTypeError(c, "finite normalized viewport point required");
            if (!frame.Ray(point, origin, direction))
                return JS_NULL;
            auto out = JS_NewObject(c);
            JS_SetPropertyStr(c, out, "origin", Vec(c, origin));
            JS_SetPropertyStr(c, out, "direction", Vec(c, direction));
            return out;
        }
        glm::vec3 point;
        if (!ReadVec(c, arg(1), point))
            return JS_ThrowTypeError(c, "finite world point required");
        const auto projected = frame.Project(point);
        auto out = JS_NewObject(c);
        for (auto field : {std::pair<const char *, double>{"x", projected.position.x},
                           {"y", projected.position.y},
                           {"depth", projected.depth},
                           {"distance", projected.distance}})
            JS_SetPropertyStr(c, out, field.first, JS_NewFloat64(c, field.second));
        JS_SetPropertyStr(c, out, "behind", JS_NewBool(c, projected.behind));
        JS_SetPropertyStr(c, out, "inside", JS_NewBool(c, projected.inside));
        return out;
    }

    if (op == "clearView") {
        world.view.reset();
        world.ResetAudioMotion();
        return JS_TRUE;
    }

    if (op == "viewRay") {
        if (world.view) {
            const auto &v = *world.view;
            auto origin = v.pose.position, direction = v.pose.rotation * glm::vec3(0, 0, -1);
            if (v.lens.projection == CameraProjectionKind::Orthographic) {
                auto frame =
                    BuildCameraFrame(v.pose.position, v.pose.rotation, v.fov, v.nearPlane,
                                     v.farPlane, v.lens, world.viewportWidth, world.viewportHeight);
                frame.Ray({.5f, .5f}, origin, direction);
            }
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "origin", Vec(c, origin));
            JS_SetPropertyStr(c, o, "direction", Vec(c, direction));
            return o;
        }
        if (!s->hasView)
            return JS_NULL;
        auto o = JS_NewObject(c);
        JS_SetPropertyStr(c, o, "origin", Vec(c, s->viewOrigin));
        JS_SetPropertyStr(c, o, "direction", Vec(c, s->viewDirection));
        return o;
    }

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

    if (op == "lightInfo" || op == "lightSettings") {
        if (!definition->light) {
            if (op == "lightInfo")
                return JS_NULL;
            return JS_ThrowTypeError(c, "entity has no light component");
        }
        auto l = *definition->light;
        if (op == "lightSettings") {
            auto patch = arg(2);
            if (!KnownOptions(c, patch,
                              {"enabled", "kind", "color", "intensity", "range", "innerConeDegrees",
                               "outerConeDegrees", "castsShadow", "shadowPriority"}))
                return JS_ThrowTypeError(c, "unknown light option");
            for (auto entry : {std::pair<const char *, float *>{"intensity", &l.intensity},
                               {"range", &l.range},
                               {"innerConeDegrees", &l.innerConeDegrees},
                               {"outerConeDegrees", &l.outerConeDegrees},
                               {"shadowPriority", &l.shadowPriority}}) {
                auto v = JS_GetPropertyStr(c, patch, entry.first);
                bool present = !JS_IsUndefined(v);
                JS_FreeValue(c, v);
                if (present && !Number(c, patch, entry.first, *entry.second))
                    return JS_ThrowTypeError(c, "finite light number required");
            }
            for (auto entry : {std::pair<const char *, bool *>{"enabled", &l.enabled},
                               {"castsShadow", &l.castsShadow}}) {
                auto v = JS_GetPropertyStr(c, patch, entry.first);
                if (!JS_IsUndefined(v)) {
                    if (!JS_IsBool(v)) {
                        JS_FreeValue(c, v);
                        return JS_ThrowTypeError(c, "light flag must be boolean");
                    }
                    *entry.second = JS_ToBool(c, v);
                }
                JS_FreeValue(c, v);
            }
            auto color = JS_GetPropertyStr(c, patch, "color");
            if (!JS_IsUndefined(color) && !ReadVec(c, color, l.color)) {
                JS_FreeValue(c, color);
                return JS_ThrowTypeError(c, "finite light color required");
            }
            JS_FreeValue(c, color);
            auto kind = JS_GetPropertyStr(c, patch, "kind");
            if (!JS_IsUndefined(kind)) {
                auto k = String(c, kind);
                if (k != "point" && k != "spot") {
                    JS_FreeValue(c, kind);
                    return JS_ThrowTypeError(c, "light kind must be point or spot");
                }
                l.kind = k == "point" ? SceneLightKind::Point : SceneLightKind::Spot;
            }
            JS_FreeValue(c, kind);
            return world.SetLight(id, l) ? JS_TRUE : JS_ThrowTypeError(c, "invalid light settings");
        }
        auto out = JS_NewObject(c);
        JS_SetPropertyStr(c, out, "kind",
                          JS_NewString(c, l.kind == SceneLightKind::Point ? "point" : "spot"));
        JS_SetPropertyStr(c, out, "color", Vec(c, l.color));
        JS_SetPropertyStr(c, out, "enabled", JS_NewBool(c, l.enabled));
        JS_SetPropertyStr(c, out, "castsShadow", JS_NewBool(c, l.castsShadow));
        for (auto entry : {std::pair<const char *, float>{"intensity", l.intensity},
                           {"range", l.range},
                           {"innerConeDegrees", l.innerConeDegrees},
                           {"outerConeDegrees", l.outerConeDegrees},
                           {"shadowPriority", l.shadowPriority}})
            JS_SetPropertyStr(c, out, entry.first, JS_NewFloat64(c, entry.second));
        return out;
    }

    if (op == "modelParts" || op == "modelPartVisible") {
        auto *parts = definition->render && world.Resources()
                          ? world.Resources()->TryGetModelParts(definition->render->meshAsset)
                          : nullptr;
        if (op == "modelPartVisible") {
            if (!JS_IsString(arg(2)) || !JS_IsBool(arg(3)))
                return JS_ThrowTypeError(c, "part identity and boolean required");
            return JS_NewBool(
                c, world.SetModelPartVisible(id, String(c, arg(2)), JS_ToBool(c, arg(3))));
        }
        if (!parts)
            return JS_NULL;
        auto array = JS_NewArray(c);
        unsigned i = 0;
        for (auto &p : *parts) {
            auto item = JS_NewObject(c);
            JS_SetPropertyStr(c, item, "identity", JS_NewString(c, p.part.c_str()));
            JS_SetPropertyStr(c, item, "materialSlot", JS_NewUint32(c, i));
            JS_SetPropertyStr(c, item, "triangles", JS_NewUint32(c, p.count / 3));
            bool visible = std::find(definition->render->hiddenParts.begin(),
                                     definition->render->hiddenParts.end(),
                                     p.part) == definition->render->hiddenParts.end();
            JS_SetPropertyStr(c, item, "visible", JS_NewBool(c, visible));
            JS_SetPropertyUint32(c, array, i++, item);
        }
        return array;
    }

    if (op == "renderVisible" || op == "rendererVisible") {
        const bool entityScope = op == "renderVisible";
        if (!entityScope && !definition->render)
            return JS_ThrowTypeError(c, "entity has no render component");
        if (argc > 2) {
            if (!JS_IsBool(arg(2)))
                return JS_ThrowTypeError(c, "visibility requires a boolean");
            return JS_NewBool(c, world.SetRenderVisible(id, entityScope, JS_ToBool(c, arg(2))));
        }
        return JS_NewBool(c, entityScope ? definition->renderVisible : definition->render->visible);
    }

    if (op == "materialInfo" || op == "materialAssign" || op == "materialOverride" ||
        op == "materialClear") {
        std::string key, part;
        unsigned slot = 0;
        if (JS_IsString(arg(2))) {
            key = String(c, arg(2));
            if (key != "*") {
                if (key.empty() || key[0] == '#')
                    return JS_ThrowTypeError(c, "stable imported part identity required");
                part = key;
                auto *parts =
                    definition->render && world.Resources()
                        ? world.Resources()->TryGetModelParts(definition->render->meshAsset)
                        : nullptr;
                if (!parts)
                    return JS_ThrowTypeError(c, "model parts not ready");
                auto it = std::find_if(parts->begin(), parts->end(),
                                       [&](auto &p) { return p.part == key; });
                if (it == parts->end())
                    return JS_ThrowTypeError(c, "unknown or removed model part");
                slot = unsigned(it - parts->begin());
            }
        } else {
            double number;
            if (JS_ToFloat64(c, &number, arg(2)) || !std::isfinite(number) || number < 0 ||
                number >= 64 || std::floor(number) != number)
                return JS_ThrowTypeError(
                    c, "material slot must be 0..63, * or a stable part identity");
            slot = unsigned(number);
            key = "#" + std::to_string(slot);
        }
        if (!definition->render)
            return JS_ThrowTypeError(c, "entity has no render component");
        auto &render = *definition->render;
        MaterialSlot value;
        if (auto it = render.runtimeMaterials.find(key); it != render.runtimeMaterials.end())
            value = it->second;
        if (op == "materialClear")
            return JS_NewBool(c, world.ClearRuntimeMaterial(id, key));
        if (op == "materialAssign") {
            if (!JS_IsString(arg(3)))
                return JS_ThrowTypeError(c, "registered material ID required");
            value.asset = String(c, arg(3));
            value.useSource = value.asset.empty();
            if (!world.SetRuntimeMaterial(id, key, value))
                return JS_ThrowTypeError(c, "missing/wrong-type material asset or invalid target");
            return JS_TRUE;
        }
        if (op == "materialOverride") {
            std::string json, error;
            if (!s->Json(arg(3), json, error) ||
                !PatchMaterialOverrides(json, value.overrides, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            if (!world.SetRuntimeMaterial(id, key, value))
                return JS_ThrowTypeError(c,
                                         "invalid material/texture asset, target or binding limit");
            return JS_TRUE;
        }
        if (part.empty() && world.Resources())
            if (auto *parts = world.Resources()->TryGetModelParts(render.meshAsset);
                parts && slot < parts->size())
                part = (*parts)[slot].part;
        auto effective = ResolveRenderMaterial(render, slot, part);
        MaterialDefinition source;
        source.model = MaterialModel::Legacy;
        std::string status = "ready", error;
        auto *resources = world.Resources();
        auto inspect = [&](const std::string &asset) {
            if (asset.empty())
                return;
            auto state = resources ? resources->StateOf(asset) : ResourceState::Unloaded;
            if (state == ResourceState::Failed || state == ResourceState::Cancelled) {
                status = "failed";
                if (error.empty())
                    error = resources->ErrorOf(asset);
            } else if (state != ResourceState::Ready && status != "failed")
                status = "pending";
        };
        if (!effective.asset.empty()) {
            inspect(effective.asset);
            if (resources)
                if (auto m = resources->TryGetMaterialDefinition(effective.asset))
                    source = *m;
        } else if (resources) {
            if (auto m = resources->TryGetMeshMaterial(render.meshAsset, slot))
                source = *m;
            else if (!render.meshAsset.empty())
                inspect(render.meshAsset);
        }
        for (const auto &t : effective.overrides.textures)
            if (t)
                inspect(*t);
        auto m = ApplyMaterialOverride(source, effective.overrides);
        auto result = JS_NewObject(c);
        JS_SetPropertyStr(c, result, "asset", JS_NewString(c, effective.asset.c_str()));
        JS_SetPropertyStr(c, result, "ready", JS_NewBool(c, status == "ready"));
        JS_SetPropertyStr(c, result, "status", JS_NewString(c, status.c_str()));
        JS_SetPropertyStr(c, result, "error", JS_NewString(c, error.c_str()));
        const char *models[] = {"legacy", "pbr", "unlit"};
        const char *alphas[] = {"opaque", "mask", "blend"};
        JS_SetPropertyStr(c, result, "model", JS_NewString(c, models[int(m.model)]));
        JS_SetPropertyStr(c, result, "alphaMode", JS_NewString(c, alphas[int(m.alpha)]));
        auto base = Vec(c, glm::vec3(m.baseColor));
        JS_SetPropertyStr(c, base, "a", JS_NewFloat64(c, m.baseColor.a));
        JS_SetPropertyStr(c, result, "baseColor", base);
        JS_SetPropertyStr(c, result, "emissive", Vec(c, m.emissive));
        for (auto p : {std::pair<const char *, float>{"metallic", m.metallic},
                       {"roughness", m.roughness},
                       {"emissiveIntensity", m.emissiveIntensity},
                       {"alphaCutoff", m.alphaCutoff},
                       {"normalStrength", m.normalStrength},
                       {"occlusionStrength", m.occlusionStrength}})
            JS_SetPropertyStr(c, result, p.first, JS_NewFloat64(c, p.second));
        JS_SetPropertyStr(c, result, "doubleSided", JS_NewBool(c, m.doubleSided));
        auto uv = [&](glm::vec2 xy) {
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "x", JS_NewFloat64(c, xy.x));
            JS_SetPropertyStr(c, o, "y", JS_NewFloat64(c, xy.y));
            return o;
        };
        JS_SetPropertyStr(c, result, "uvScale", uv(m.uvScale));
        JS_SetPropertyStr(c, result, "uvOffset", uv(m.uvOffset));
        JS_SetPropertyStr(c, result, "overridden",
                          JS_NewBool(c, render.runtimeMaterials.count(key) > 0));
        auto textures = JS_NewObject(c);
        const char *names[] = {"baseColor", "metallicRoughness", "normal", "occlusion", "emissive"};
        for (size_t i = 0; i < 5; ++i)
            JS_SetPropertyStr(c, textures, names[i], JS_NewString(c, m.maps[i].asset.c_str()));
        JS_SetPropertyStr(c, result, "textures", textures);
        return result;
    }

    if (op == "tileSet") {
        if (!s->fixed || s->inPresentation || s->stopping || world.NetworkClient())
            return JS_ThrowTypeError(c, "tile edits require an authoritative fixed callback");
        double x = 0, y = 0;
        auto readCell = [&](JSValueConst value, double &coordinate) {
            return JS_IsNumber(value) && !JS_ToFloat64(c, &coordinate, value) &&
                   std::isfinite(coordinate) && coordinate >= -512 && coordinate <= 512 &&
                   coordinate == std::floor(coordinate);
        };
        if (!JS_IsString(arg(2)) || !readCell(arg(3), x) || !readCell(arg(4), y) ||
            !(JS_IsNull(arg(5)) || JS_IsString(arg(5))) ||
            !KnownOptions(c, arg(6), {"flipX", "flipY"}))
            return JS_ThrowTypeError(
                c, "tile edit requires a layer, integer coordinates, and a palette frame or null");
        auto frame = JS_IsNull(arg(5)) ? std::string{} : String(c, arg(5));
        if (JS_IsString(arg(5)) && (frame.empty() || frame.size() > 128))
            return JS_ThrowTypeError(c, "tile frame must contain 1 to 128 bytes");
        bool flips[2]{};
        const char *keys[] = {"flipX", "flipY"};
        for (unsigned i = 0; i < 2; ++i) {
            auto value = JS_GetPropertyStr(c, arg(6), keys[i]);
            const bool valid = JS_IsUndefined(value) || JS_IsBool(value);
            flips[i] = JS_IsBool(value) && JS_ToBool(c, value);
            JS_FreeValue(c, value);
            if (!valid)
                return JS_ThrowTypeError(c, "tile flip flags must be boolean");
        }
        std::string error;
        if (!world.EditTilemapCell(id, String(c, arg(2)), {int(x), int(y)}, frame, flips[0],
                                   flips[1], error))
            return JS_ThrowTypeError(c, "%s", error.c_str());
        return JS_TRUE;
    }

    if (op == "spriteAnimationInfo" || op == "spriteAnimationSet") {
        const auto *d = world.RuntimeDefinition(id);
        if (!d || !d->render || d->render->shape != SceneShape::Sprite)
            return op == "spriteAnimationInfo" ? JS_NULL : JS_FALSE;
        if (op == "spriteAnimationInfo") {
            if (!d->render->spritePlayback)
                return JS_NULL;
            const auto &p = *d->render->spritePlayback;
            auto out = JS_NewObject(c);
            for (const auto &field : std::vector<std::pair<const char *, std::string>>{
                     {"sheet", p.sheet}, {"clip", p.clip}, {"frame", p.frame}})
                JS_SetPropertyStr(c, out, field.first, JS_NewString(c, field.second.c_str()));
            JS_SetPropertyStr(c, out, "time", JS_NewFloat64(c, p.time));
            JS_SetPropertyStr(c, out, "speed", JS_NewFloat64(c, p.speed));
            JS_SetPropertyStr(c, out, "playing", JS_NewBool(c, p.playing));
            JS_SetPropertyStr(c, out, "loop", JS_NewBool(c, p.loop));
            std::string resourceError, frameKey;
            auto *resources = world.Resources();
            auto sheet = resources ? resources->GetSpriteSheet(p.sheet, resourceError) : nullptr;
            SpriteSettings sample = d->render->sprite;
            const bool ready =
                sheet && SampleSpriteFrame(*sheet, p, sample, resourceError, &frameKey);
            JS_SetPropertyStr(c, out, "ready", JS_NewBool(c, ready));
            JS_SetPropertyStr(c, out, "sampledFrame", JS_NewString(c, frameKey.c_str()));
            const auto *step = world.SpriteAnimationObservation(id);
            auto events = JS_NewArray(c);
            unsigned i = 0;
            if (step)
                for (const auto &hit : step->events) {
                    auto e = JS_NewObject(c);
                    JS_SetPropertyStr(c, e, "id", JS_NewString(c, hit.event.id.c_str()));
                    JS_SetPropertyStr(c, e, "name", JS_NewString(c, hit.event.name.c_str()));
                    JS_SetPropertyStr(c, e, "data", JS_NewString(c, hit.event.data.c_str()));
                    JS_SetPropertyStr(c, e, "time", JS_NewFloat64(c, hit.event.time));
                    JS_SetPropertyStr(c, e, "reverse", JS_NewBool(c, hit.reverse));
                    JS_SetPropertyUint32(c, events, i++, e);
                }
            JS_SetPropertyStr(c, out, "events", events);
            JS_SetPropertyStr(c, out, "finished", JS_NewBool(c, step && step->finished));
            JS_SetPropertyStr(c, out, "overflow", JS_NewBool(c, step && step->overflow));
            const auto error = step && !step->error.empty() ? step->error : resourceError;
            JS_SetPropertyStr(c, out, "error",
                              JS_NewString(c, error == "loading" ? "" : error.c_str()));
            return out;
        }
        auto p = d->render->spritePlayback.value_or(SpritePlayback{});
        auto value = arg(2);
        if (!KnownOptions(c, value, {"sheet", "clip", "frame", "time", "speed", "playing", "loop"}))
            return JS_ThrowTypeError(c, "unknown sprite animation setting");
        for (auto field : std::vector<std::pair<const char *, std::string *>>{
                 {"sheet", &p.sheet}, {"clip", &p.clip}, {"frame", &p.frame}}) {
            auto v = JS_GetPropertyStr(c, value, field.first);
            if (!JS_IsUndefined(v)) {
                if (!JS_IsString(v)) {
                    JS_FreeValue(c, v);
                    return JS_ThrowTypeError(c, "sprite sheet/clip/frame must be strings");
                }
                const char *text = JS_ToCString(c, v);
                if (!text) {
                    JS_FreeValue(c, v);
                    return JS_EXCEPTION;
                }
                *field.second = text;
                JS_FreeCString(c, text);
            }
            JS_FreeValue(c, v);
        }
        for (auto field : std::vector<std::pair<const char *, bool *>>{{"playing", &p.playing},
                                                                       {"loop", &p.loop}}) {
            auto v = JS_GetPropertyStr(c, value, field.first);
            if (!JS_IsUndefined(v)) {
                if (!JS_IsBool(v)) {
                    JS_FreeValue(c, v);
                    return JS_ThrowTypeError(c, "playing/loop must be Boolean");
                }
                *field.second = JS_ToBool(c, v);
            }
            JS_FreeValue(c, v);
        }
        auto time = JS_GetPropertyStr(c, value, "time");
        if (!JS_IsUndefined(time)) {
            if (!JS_IsNumber(time) || JS_ToFloat64(c, &p.time, time) < 0) {
                JS_FreeValue(c, time);
                return JS_ThrowTypeError(c, "time must be finite");
            }
        }
        JS_FreeValue(c, time);
        auto speed = JS_GetPropertyStr(c, value, "speed");
        if (!JS_IsUndefined(speed)) {
            double n = 0;
            if (!JS_IsNumber(speed) || JS_ToFloat64(c, &n, speed) < 0 || !std::isfinite(n)) {
                JS_FreeValue(c, speed);
                return JS_ThrowTypeError(c, "speed must be finite");
            }
            p.speed = float(n);
        }
        JS_FreeValue(c, speed);
        if (!ValidSpritePlayback(p))
            return JS_ThrowTypeError(c, "invalid sprite animation time/speed/identity");
        std::string error;
        if (!world.SetSpritePlayback(id, p, error))
            return JS_ThrowTypeError(c, "sprite animation: %s", error.c_str());
        return JS_TRUE;
    }

    if (op == "spriteInfo" || op == "spriteSet") {
        const auto *definition = world.RuntimeDefinition(id);
        if (!definition || !definition->render || definition->render->shape != SceneShape::Sprite)
            return op == "spriteInfo" ? JS_NULL : JS_FALSE;
        if (op == "spriteInfo")
            return SpriteValue(c, definition->render->sprite);
        auto sprite = definition->render->sprite;
        if (!ReadSpriteValues(c, arg(2), sprite))
            return JS_ThrowTypeError(
                c, "invalid sprite options, dimensions, pivot, image rectangle or sort keys");
        return JS_NewBool(c, world.SetSprite(id, sprite));
    }

    if (op == "cameraPresentation") {
        auto options = arg(2);
        if (!KnownOptions(c, options, {"fov", "updateEveryFrames", "renderLayers"}))
            return JS_ThrowTypeError(c, "unknown camera presentation setting");
        for (auto &camera : world.PresentationCameras())
            if (camera.id == id) {
                auto settings = camera.settings;
                for (const char *key : {"fov", "updateEveryFrames"}) {
                    auto property = JS_GetPropertyStr(c, options, key);
                    const bool present = !JS_IsUndefined(property);
                    JS_FreeValue(c, property);
                    if (!present)
                        continue;
                    float value;
                    if (!Number(c, options, key, value))
                        return JS_ThrowTypeError(c, "finite camera setting required");
                    if (std::string_view(key) == "fov") {
                        if (value <= 0 || value >= 179)
                            return JS_ThrowRangeError(
                                c, "camera fov must be between 0 and 179 degrees");
                        settings.verticalFovDegrees = value;
                    } else {
                        if (value < 1 || value > 10000 || std::floor(value) != value)
                            return JS_ThrowRangeError(
                                c, "camera updateEveryFrames must be an integer from 1 to 10000");
                        settings.updateEveryFrames = int(value);
                    }
                }
                auto layers = JS_GetPropertyStr(c, options, "renderLayers");
                const bool valid =
                    JS_IsUndefined(layers) ||
                    namedLayerMask(layers, world.Categories().render, settings.renderMask);
                JS_FreeValue(c, layers);
                if (!valid)
                    return JS_ThrowTypeError(
                        c, "renderLayers must contain registered render layer names or be null");
                camera.settings = settings;
                return JS_TRUE;
            }
        return JS_FALSE;
    }

    if (op == "cameraProjection") {
        // Lens parsing is shared with script-owned main and local-player views.
        float nearPlane = 0, farPlane = 0;
        auto range = arg(2);
        if (!Number(c, range, "near", nearPlane) || !Number(c, range, "far", farPlane) ||
            !ValidCameraRange(nearPlane, farPlane))
            return JS_ThrowTypeError(c, "valid near/far range required");
        // Portal views: optional off-axis frustum and oblique clip plane. Omitted keeps the current
        // value, null clears.
        int frustumMode = 0, planeMode = 0;
        glm::vec4 frustum{0};
        glm::vec3 normal{0, 0, 1}, point{0};
        {
            auto v = JS_GetPropertyStr(c, range, "frustum");
            bool ok = true;
            if (JS_IsNull(v))
                frustumMode = 1;
            else if (!JS_IsUndefined(v)) {
                frustumMode = 2;
                ok = KnownOptions(c, v, {"left", "right", "bottom", "top"}) &&
                     Number(c, v, "left", frustum.x) && Number(c, v, "right", frustum.y) &&
                     Number(c, v, "bottom", frustum.z) && Number(c, v, "top", frustum.w) &&
                     ValidCameraFrustum(frustum);
            }
            JS_FreeValue(c, v);
            if (!ok)
                return JS_ThrowTypeError(
                    c, "frustum requires finite left<right and bottom<top at the near plane");
        }
        {
            auto v = JS_GetPropertyStr(c, range, "clipPlane");
            bool ok = true;
            if (JS_IsNull(v))
                planeMode = 1;
            else if (!JS_IsUndefined(v)) {
                planeMode = 2;
                auto n = JS_GetPropertyStr(c, v, "normal"), q = JS_GetPropertyStr(c, v, "point");
                ok = KnownOptions(c, v, {"normal", "point"}) && ReadVec(c, n, normal) &&
                     ReadVec(c, q, point) && ValidCameraPlane(glm::vec4(normal, 0));
                JS_FreeValue(c, n);
                JS_FreeValue(c, q);
            }
            JS_FreeValue(c, v);
            if (!ok)
                return JS_ThrowTypeError(
                    c, "clipPlane requires finite {normal,point} with a non-zero normal");
        }
        for (auto &camera : world.PresentationCameras())
            if (camera.id == id) {
                auto lens = camera.settings.lens;
                if (!ReadCameraLens(c, range, lens))
                    return JS_ThrowTypeError(c, "invalid camera projection/pixel settings");
                if (lens.projection == CameraProjectionKind::Orthographic &&
                    ((frustumMode != 1 && (camera.hasFrustum || frustumMode == 2)) ||
                     (planeMode != 1 && (camera.hasClipPlane || planeMode == 2))))
                    return JS_ThrowTypeError(c,
                                             "orthographic cameras do not support portal frustums "
                                             "or oblique clipping; clear them explicitly");
                camera.settings.lens = lens;
                camera.settings.nearPlane = nearPlane;
                camera.settings.farPlane = farPlane;
                if (frustumMode == 1)
                    camera.hasFrustum = false;
                else if (frustumMode == 2) {
                    camera.hasFrustum = true;
                    camera.frustum = frustum;
                }
                if (planeMode == 1)
                    camera.hasClipPlane = false;
                else if (planeMode == 2) {
                    camera.hasClipPlane = true;
                    camera.clipNormal = glm::normalize(normal);
                    camera.clipPoint = point;
                }
                return JS_TRUE;
            }
        return JS_FALSE;
    }

    if (op == "cameraInfo") {
        for (const auto &camera : world.PresentationCameras())
            if (camera.id == id) {
                auto o = JS_NewObject(c);
                JS_SetPropertyStr(c, o, "lens", CameraLensValue(c, camera.settings.lens));
                JS_SetPropertyStr(c, o, "enabled", JS_NewBool(c, camera.settings.enabled));
                JS_SetPropertyStr(c, o, "width", JS_NewInt32(c, camera.settings.width));
                JS_SetPropertyStr(c, o, "height", JS_NewInt32(c, camera.settings.height));
                JS_SetPropertyStr(c, o, "near", JS_NewFloat64(c, camera.settings.nearPlane));
                JS_SetPropertyStr(c, o, "far", JS_NewFloat64(c, camera.settings.farPlane));
                JS_SetPropertyStr(c, o, "targetWidth", JS_NewInt32(c, camera.targetSize.x));
                JS_SetPropertyStr(c, o, "targetHeight", JS_NewInt32(c, camera.targetSize.y));
                JS_SetPropertyStr(c, o, "targetError",
                                  camera.error.empty() ? JS_NULL
                                                       : JS_NewString(c, camera.error.c_str()));
                JS_SetPropertyStr(c, o, "fov",
                                  JS_NewFloat64(c, camera.settings.verticalFovDegrees));
                JS_SetPropertyStr(c, o, "updateEveryFrames",
                                  JS_NewInt32(c, camera.settings.updateEveryFrames));
                JSValue layers = JS_NULL;
                if (camera.settings.renderMask != kAllCategories) {
                    layers = JS_NewArray(c);
                    unsigned index = 0;
                    for (const auto &[bit, name] : world.Categories().render.names)
                        if (camera.settings.renderMask & CategoryBit(bit))
                            JS_SetPropertyUint32(c, layers, index++, JS_NewString(c, name.c_str()));
                }
                JS_SetPropertyStr(c, o, "renderLayers", layers);
                if (camera.hasFrustum) {
                    auto f = JS_NewObject(c);
                    JS_SetPropertyStr(c, f, "left", JS_NewFloat64(c, camera.frustum.x));
                    JS_SetPropertyStr(c, f, "right", JS_NewFloat64(c, camera.frustum.y));
                    JS_SetPropertyStr(c, f, "bottom", JS_NewFloat64(c, camera.frustum.z));
                    JS_SetPropertyStr(c, f, "top", JS_NewFloat64(c, camera.frustum.w));
                    JS_SetPropertyStr(c, o, "frustum", f);
                } else
                    JS_SetPropertyStr(c, o, "frustum", JS_NULL);
                if (camera.hasClipPlane) {
                    auto f = JS_NewObject(c);
                    JS_SetPropertyStr(c, f, "normal", Vec(c, camera.clipNormal));
                    JS_SetPropertyStr(c, f, "point", Vec(c, camera.clipPoint));
                    JS_SetPropertyStr(c, o, "clipPlane", f);
                } else
                    JS_SetPropertyStr(c, o, "clipPlane", JS_NULL);
                return o;
            }
        return JS_NULL;
    }

    if (op == "cameraTargetSize") {
        auto dimension = [&](JSValueConst value, double &out) {
            return JS_IsNumber(value) && !JS_ToFloat64(c, &out, value) && std::isfinite(out) &&
                   out >= 1 && out <= 4096 && out == std::floor(out);
        };
        double width = 0, height = 0;
        if (!dimension(arg(2), width) || !dimension(arg(3), height))
            return JS_ThrowRangeError(c,
                                      "camera target dimensions must be integers from 1 to 4096");
        for (auto &camera : world.PresentationCameras())
            if (camera.id == id) {
                camera.settings.width = int(width);
                camera.settings.height = int(height);
                return JS_TRUE;
            }
        return JS_FALSE;
    }

    if (op == "camera" && !JS_IsBool(arg(2)))
        return JS_ThrowTypeError(c, "camera enabled must be boolean");

    if (op == "camera") {
        for (auto &camera : world.PresentationCameras())
            if (camera.id == id) {
                camera.settings.enabled = JS_ToBool(c, arg(2));
                return JS_TRUE;
            }
        return JS_FALSE;
    }
    return JS_ThrowTypeError(c, "unhandled Rendering operation");
}
} // namespace
void RegisterRenderingBindings(BindingRegistry &registry) {
    registry.Add("Rendering",
                 {"appearanceReset",
                  "appearanceInfo",
                  "appearanceSet",
                  "setView",
                  "viewportSize",
                  "projectViewport",
                  "viewportRay",
                  "clearView",
                  "viewRay",
                  "lightInfo",
                  "lightSettings",
                  "modelParts",
                  "modelPartVisible",
                  "renderVisible",
                  "rendererVisible",
                  "materialInfo",
                  "materialAssign",
                  "materialOverride",
                  "materialClear",
                  "tileSet",
                  "spriteAnimationInfo",
                  "spriteAnimationSet",
                  "spriteInfo",
                  "spriteSet",
                  "cameraPresentation",
                  "cameraProjection",
                  "cameraInfo",
                  "cameraTargetSize",
                  "camera"},
                 HandleRendering);
}
} // namespace ScriptBindings
