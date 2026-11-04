#include "../SceneFingerprint.h"
#include "ScriptRuntimeInternal.h"

ScriptMetadataStats ScriptSystem::MetadataStats() {
    return {metadataParserConstructions.load(), metadataReferenceInspections.load()};
}
ScriptSystem::Impl &ScriptSystem::PropertyVM() {
    static thread_local Impl vm(nullptr, nullptr, true);
    return vm;
}
bool ScriptSystem::ValidateJson(const std::string &text, std::string &error, bool requireObject) {
    error.clear();
    if (text.size() > 65536) {
        error = "JSON exceeds 64KiB";
        return false;
    }
    auto &vm = PropertyVM();
    vm.polls = 0;
    auto value = JS_ParseJSON(vm.ctx, text.data(), text.size(), "JSON");
    if (JS_IsException(value)) {
        error = Exception(vm.ctx);
        return false;
    }
    std::string canonical;
    bool ok = (!requireObject || JS_IsObject(value)) && vm.Json(value, canonical, error);
    if (!ok && error.empty())
        error = "JSON root must be object";
    JS_FreeValue(vm.ctx, value);
    return ok;
}
bool ScriptSystem::Inspect(const AssetDatabase &assets, const std::string &asset,
                           std::string &schema, std::string &error) {
    Impl vm(nullptr, &assets);
    vm.polls = 0;
    auto ns = vm.Namespace(asset);
    if (JS_IsException(ns)) {
        error = Exception(vm.ctx);
        return false;
    }
    auto value = JS_GetPropertyStr(vm.ctx, ns, "properties");
    JS_FreeValue(vm.ctx, ns);
    if (JS_IsUndefined(value)) {
        JS_FreeValue(vm.ctx, value);
        schema = "{}";
        return true;
    }
    bool ok = vm.Json(value, schema, error);
    JS_FreeValue(vm.ctx, value);
    return ok;
}
bool ScriptSystem::ReadProperties(const std::string &schema, const std::string &values,
                                  std::vector<ScriptProperty> &out, std::string &error) {
    if (!ValidateJson(schema, error) || !ValidateJson(values, error))
        return false;
    auto &vm = PropertyVM();
    auto *c = vm.ctx;
    auto definition = JS_ParseJSON(c, schema.data(), schema.size(), "schema"),
         data = JS_ParseJSON(c, values.data(), values.size(), "properties");
    JSPropertyEnum *keys = nullptr;
    uint32_t count = 0;
    bool ok = JS_GetOwnPropertyNames(c, &keys, &count, definition, JS_GPN_STRING_MASK) == 0;
    std::vector<ScriptProperty> fields;
    for (uint32_t i = 0; ok && i < count; ++i) {
        ScriptProperty field;
        auto name = JS_AtomToString(c, keys[i].atom);
        field.name = String(c, name);
        JS_FreeValue(c, name);
        auto spec = JS_GetProperty(c, definition, keys[i].atom);
        auto type = JS_GetPropertyStr(c, spec, "type");
        field.type = String(c, type);
        JS_FreeValue(c, type);
        auto value = JS_GetProperty(c, data, keys[i].atom);
        if (JS_IsUndefined(value)) {
            JS_FreeValue(c, value);
            value = JS_GetPropertyStr(c, spec, "default");
        }
        if (field.type == "number") {
            double number = 0;
            ok =
                JS_IsNumber(value) && JS_ToFloat64(c, &number, value) == 0 && std::isfinite(number);
            field.number = number;
        } else if (field.type == "boolean") {
            ok = JS_IsBool(value);
            field.boolean = JS_ToBool(c, value);
        } else if (field.type == "entity") {
            if (JS_IsNull(value)) {
                field.text = "0";
            } else {
                auto id = JS_GetPropertyStr(c, value, "entity");
                ok = JS_IsObject(value) && JS_IsString(id);
                field.text = String(c, id);
                try {
                    size_t end = 0;
                    auto n = std::stoull(field.text, &end);
                    ok = ok && n > 0 && end == field.text.size();
                } catch (...) {
                    ok = false;
                }
                JS_FreeValue(c, id);
            }
        } else if (field.type == "string") {
            ok = JS_IsString(value);
            field.text = String(c, value);
        } else
            ok = false;
        JS_FreeValue(c, value);
        JS_FreeValue(c, spec);
        fields.push_back(field);
        if (!ok)
            error = "invalid schema/default/value for " + field.name;
    }
    JS_FreePropertyEnum(c, keys, count);
    keys = nullptr;
    count = 0;
    JS_GetOwnPropertyNames(c, &keys, &count, data, JS_GPN_STRING_MASK);
    for (uint32_t i = 0; ok && i < count; ++i) {
        auto name = JS_AtomToString(c, keys[i].atom);
        auto text = String(c, name);
        JS_FreeValue(c, name);
        if (std::none_of(fields.begin(), fields.end(),
                         [&](const auto &f) { return f.name == text; })) {
            error = "undeclared property " + text;
            ok = false;
        }
    }
    JS_FreePropertyEnum(c, keys, count);
    JS_FreeValue(c, definition);
    JS_FreeValue(c, data);
    if (ok)
        out = std::move(fields);
    return ok;
}
std::string ScriptSystem::WriteProperties(const std::vector<ScriptProperty> &values) {
    auto &vm = PropertyVM();
    auto o = JS_NewObject(vm.ctx);
    for (const auto &field : values) {
        if (field.type == "entity") {
            auto v = field.text == "0" ? JS_NULL : JS_NewObject(vm.ctx);
            if (!JS_IsNull(v))
                JS_SetPropertyStr(vm.ctx, v, "entity", JS_NewString(vm.ctx, field.text.c_str()));
            JS_SetPropertyStr(vm.ctx, o, field.name.c_str(), v);
            continue;
        }
        auto v = field.type == "number"    ? JS_NewFloat64(vm.ctx, field.number)
                 : field.type == "boolean" ? JS_NewBool(vm.ctx, field.boolean)
                                           : JS_NewString(vm.ctx, field.text.c_str());
        JS_SetPropertyStr(vm.ctx, o, field.name.c_str(), v);
    }
    auto json = JS_JSONStringify(vm.ctx, o, JS_UNDEFINED, JS_UNDEFINED);
    auto text = String(vm.ctx, json);
    JS_FreeValue(vm.ctx, json);
    JS_FreeValue(vm.ctx, o);
    return text;
}
bool ScriptSystem::SourceFingerprint(const AssetDatabase &assets, const Scene &scene,
                                     std::string &digest, std::string &error, bool strict) {
    Impl vm(nullptr, &assets);
    std::set<std::string> roots;
    std::map<std::string, std::string> sources;
    for (const auto &object : scene.Objects())
        for (const auto &slot : object.scripts) {
            const auto *record = assets.Find(slot.asset);
            if (!slot.id || !ValidateJson(slot.properties, error))
                return false;
            if (!record || record->missing || record->type != AssetType::Script) {
                if (strict) {
                    error = "missing script asset " + slot.asset;
                    return false;
                }
                sources[slot.asset] = "MISSING";
                continue;
            }
            roots.insert(record->relativePath);
        }
    for (const auto &root : roots) {
        vm.polls = 0;
        auto *module = vm.Module(root);
        if (!module || JS_ResolveModule(vm.ctx, JS_MKPTR(JS_TAG_MODULE, module)) < 0) {
            error = root + ": " + Exception(vm.ctx);
            if (strict)
                return false;
            break;
        }
    }
    // Strict project-code compatibility also covers scripts loaded by future
    // runtime SpawnPrefab calls. IDs/content are portable; absolute paths are
    // not.
    for (const auto &entry : assets.Records())
        if (entry.second.type == AssetType::Script) {
            const auto &record = entry.second;
            if (record.missing) {
                sources[record.id] = "MISSING";
                continue;
            }
            std::ifstream file(record.path);
            std::ostringstream data;
            data << file.rdbuf();
            sources[record.id] = data.str();
        }
    std::string bytes = "Judas.ScriptSources.1";
    for (const auto &entry : sources) {
        bytes += entry.first;
        bytes += SceneFingerprintSha256(entry.second);
    }
    digest = SceneFingerprintSha256(bytes);
    error.clear();
    return true;
}

namespace {
// Tagged authored references are remapped through existing scene/prefab IDs.
// Arbitrary numeric/string gameplay properties are never interpreted as IDs.
void PropertyReferences(JSContext *c, JSValueConst object,
                        const std::function<void(const std::string &, JSValueConst)> &visit) {
    JSPropertyEnum *keys = nullptr;
    uint32_t count = 0;
    if (!JS_IsObject(object) ||
        JS_GetOwnPropertyNames(c, &keys, &count, object, JS_GPN_STRING_MASK) < 0)
        return;
    for (uint32_t i = 0; i < count; ++i) {
        auto name = JS_AtomToString(c, keys[i].atom);
        auto key = String(c, name);
        JS_FreeValue(c, name);
        auto value = JS_GetProperty(c, object, keys[i].atom);
        if (JS_IsObject(value)) {
            auto id = JS_GetPropertyStr(c, value, "entity");
            if (JS_IsString(id))
                visit(key, id);
            JS_FreeValue(c, id);
        }
        JS_FreeValue(c, value);
    }
    JS_FreePropertyEnum(c, keys, count);
}
} // namespace
std::vector<SceneObjectId> ScriptSystem::PropertyEntities(const std::string &values) {
    ++metadataReferenceInspections;
    auto &vm = PropertyVM();
    auto o = JS_ParseJSON(vm.ctx, values.data(), values.size(), "properties");
    std::vector<SceneObjectId> result;
    if (JS_IsException(o)) {
        Exception(vm.ctx);
        JS_FreeValue(vm.ctx, o);
        return result;
    }
    PropertyReferences(vm.ctx, o, [&](const std::string &, JSValueConst id) {
        try {
            size_t end = 0;
            auto text = String(vm.ctx, id);
            auto value = std::stoull(text, &end);
            if (value && end == text.size())
                result.push_back(value);
        } catch (...) {
        }
    });
    JS_FreeValue(vm.ctx, o);
    return result;
}
std::string ScriptSystem::RemapPropertyEntities(const std::string &values,
                                                const std::map<SceneObjectId, SceneObjectId> &ids) {
    auto &vm = PropertyVM();
    auto o = JS_ParseJSON(vm.ctx, values.data(), values.size(), "properties");
    if (JS_IsException(o)) {
        Exception(vm.ctx);
        JS_FreeValue(vm.ctx, o);
        return values;
    }
    PropertyReferences(vm.ctx, o, [&](const std::string &key, JSValueConst id) {
        try {
            auto value = std::stoull(String(vm.ctx, id));
            auto found = ids.find(value);
            if (found != ids.end()) {
                auto ref = JS_NewObject(vm.ctx);
                JS_SetPropertyStr(vm.ctx, ref, "entity",
                                  JS_NewString(vm.ctx, std::to_string(found->second).c_str()));
                JS_SetPropertyStr(vm.ctx, o, key.c_str(), ref);
            }
        } catch (...) {
        }
    });
    auto json = JS_JSONStringify(vm.ctx, o, JS_UNDEFINED, JS_UNDEFINED);
    auto result = String(vm.ctx, json);
    JS_FreeValue(vm.ctx, json);
    JS_FreeValue(vm.ctx, o);
    return result;
}
bool ScriptSystem::SetPropertyEntity(std::string &values, const std::string &name,
                                     SceneObjectId id) {
    auto &vm = PropertyVM();
    auto o = JS_ParseJSON(vm.ctx, values.data(), values.size(), "properties");
    if (!JS_IsObject(o)) {
        if (JS_IsException(o))
            Exception(vm.ctx);
        JS_FreeValue(vm.ctx, o);
        return false;
    }
    auto ref = id ? JS_NewObject(vm.ctx) : JS_NULL;
    if (id)
        JS_SetPropertyStr(vm.ctx, ref, "entity", JS_NewString(vm.ctx, std::to_string(id).c_str()));
    JS_SetPropertyStr(vm.ctx, o, name.c_str(), ref);
    auto json = JS_JSONStringify(vm.ctx, o, JS_UNDEFINED, JS_UNDEFINED);
    values = String(vm.ctx, json);
    JS_FreeValue(vm.ctx, json);
    JS_FreeValue(vm.ctx, o);
    return true;
}
