#pragma once
#include "../CameraProjection.h"
#include "../Scene.h"

#include "quickjs.h"
#include <glm/glm.hpp>
#include <cmath>
#include <string>

// Keep coercion, bounds and QuickJS ownership identical across native bridges.
namespace ScriptBindingValues {
inline std::string String(JSContext *c, JSValueConst v) {
    size_t length = 0;
    const char *p = JS_ToCStringLen(c, &length, v);
    if (!p)
        return {};
    std::string s(p, length);
    JS_FreeCString(c, p);
    return s;
}
inline bool Number(JSContext *c, JSValueConst o, const char *key, float &value) {
    auto v = JS_GetPropertyStr(c, o, key);
    double n = 0;
    bool ok = JS_ToFloat64(c, &n, v) == 0 && std::isfinite(n) && std::abs(n) <= 1e20;
    JS_FreeValue(c, v);
    value = static_cast<float>(n);
    return ok;
}
inline bool ReadVec(JSContext *c, JSValueConst o, glm::vec3 &v) {
    return Number(c, o, "x", v.x) && Number(c, o, "y", v.y) && Number(c, o, "z", v.z);
}
} // namespace ScriptBindingValues

#include <initializer_list>

namespace ScriptBindingValues {
JSValue Vec(JSContext *, glm::vec3);
bool KnownOptions(JSContext *, JSValueConst, std::initializer_list<const char *>);
bool ReadCameraLens(JSContext *, JSValueConst, CameraLens &);
JSValue CameraLensValue(JSContext *, const CameraLens &);
bool ReadSpriteValues(JSContext *, JSValueConst, SpriteSettings &);
JSValue SpriteValue(JSContext *, const SpriteSettings &);
bool ReadPlanarValues(JSContext *, JSValueConst, std::shared_ptr<const PlanarCollider> &);
JSValue PlanarValue(JSContext *, const PlanarCollider &);
} // namespace ScriptBindingValues
