#include "BindingValues.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
namespace ScriptBindingValues {
SceneObjectId ReadEntityId(JSContext *c, JSValueConst value) {
    std::size_t length = 0;
    const char* text = JS_ToCStringLen(c, &length, value);
    if (!text)
        return 0;
    char* end = nullptr;
    errno = 0;
    const auto id = std::strtoull(text, &end, 10);
    const bool valid = end != text && end == text + length && errno != ERANGE;
    JS_FreeCString(c, text);
    return valid ? id : 0;
}

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
