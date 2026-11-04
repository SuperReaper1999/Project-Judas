#include "BindingValues.h"
#include <cstring>
namespace ScriptBindingValues {
JSValue Vec(JSContext *c, glm::vec3 v) {
    auto o = JS_NewObject(c);
    JS_SetPropertyStr(c, o, "x", JS_NewFloat64(c, v.x));
    JS_SetPropertyStr(c, o, "y", JS_NewFloat64(c, v.y));
    JS_SetPropertyStr(c, o, "z", JS_NewFloat64(c, v.z));
    return o;
}
bool KnownOptions(JSContext *c, JSValueConst value, std::initializer_list<const char *> allowed) {
    if (!JS_IsObject(value))
        return false;
    JSPropertyEnum *properties = nullptr;
    uint32_t count = 0;
    if (JS_GetOwnPropertyNames(c, &properties, &count, value,
                               JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY))
        return false;
    bool valid = true;
    for (uint32_t i = 0; i < count; ++i) {
        const char *key = JS_AtomToCString(c, properties[i].atom);
        bool known = false;
        for (auto name : allowed)
            known |= key && std::strcmp(key, name) == 0;
        valid &= known;
        JS_FreeCString(c, key);
        JS_FreeAtom(c, properties[i].atom);
    }
    js_free(c, properties);
    return valid;
}
} // namespace ScriptBindingValues
