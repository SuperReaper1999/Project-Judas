#pragma once

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
