#include "BuildIdentity.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "scripting/ScriptRuntimeInternal.h"
#include "scripting/modules/AI.h"

namespace ScriptRuntimeInternal {
std::atomic<uint64_t> metadataParserConstructions{0}, metadataReferenceInspections{0};
std::string Exception(JSContext *c) {
    auto e = JS_GetException(c);
    auto stack = JS_GetPropertyStr(c, e, "stack");
    auto s = String(c, e) + "\n" + String(c, stack);
    JS_FreeValue(c, stack);
    JS_FreeValue(c, e);
    return s;
}
} // namespace ScriptRuntimeInternal

JSValue ScriptSystem::Impl::Native(JSContext *c, JSValueConst, int argc, JSValueConst *argv) {
    auto &self = *static_cast<Impl *>(JS_GetContextOpaque(c));
    if (argc < 1)
        return JS_ThrowTypeError(c, "missing operation");
    const auto operation = String(c, argv[0]);
    ScriptBindings::BindingContext context{c,
                                           self.world,
                                           self.assets,
                                           self,
                                           self.bindingStates,
                                           self.input,
                                           self.fixed,
                                           self.delta,
                                           self.inUI,
                                           self.inPresentation,
                                           self.presentationAlpha,
                                           self.hasView,
                                           self.viewOrigin,
                                           self.viewDirection,
                                           self.currentOwner,
                                           self.currentSlot,
                                           self.stopping,
                                           self.retiringAI,
                                           self.animationControl};
    return ScriptBindings::BindingRegistry::Default().Dispatch(context, operation, argc, argv);
}
JSValue ScriptSystem::Impl::RuntimeOperation(const std::string &op, int argc, JSValueConst *argv) {
    auto *s = this;
    auto *c = ctx;
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };
    if (op == "modCatalog") {
        const std::string empty = "{\"active\":[],\"contributions\":[],\"exports\":{}}";
        const auto &data =
            s->assets && !s->assets->ModCatalog().empty() ? s->assets->ModCatalog() : empty;
        return JS_ParseJSON(c, data.data(), data.size(), "mod catalog");
    }
    if (op == "log") {
        auto text = String(c, arg(1));
        std::fprintf(stdout, "JS: %s\n", text.c_str());
        if (s->debug) {
            if (s->consoleMessages.size() >= 256)
                s->consoleMessages.erase(s->consoleMessages.begin());
            s->consoleMessages.push_back(text.substr(0, 4096));
        }
        return JS_UNDEFINED;
    }
    if (op == "profileScope") {
        if (!JS_IsString(arg(1)) || !JS_IsFunction(c, arg(2)))
            return JS_ThrowTypeError(c, "profiler.scope requires a string and callback");
        ProfileScope scope(s->CustomLabel(arg(1)));
        return JS_Call(c, arg(2), JS_UNDEFINED, 0,
                       nullptr); // exactly once; exception and interrupt pass through
    }
    if (op == "profileCounter") {
        double value;
        if (!JS_IsString(arg(1)) || JS_ToFloat64(c, &value, arg(2)) != 0 || !std::isfinite(value))
            return JS_ThrowTypeError(c, "profiler.counter requires a string and finite value");
        auto mode = String(c, arg(3));
        if (mode != "sum" && mode != "latest" && mode != "max")
            return JS_ThrowTypeError(c, "counter mode must be sum, latest or max");
        PerformanceProfiler::Get().Counter(s->CustomLabel(arg(1)), value,
                                           mode == "sum"   ? ProfileCounterMode::Sum
                                           : mode == "max" ? ProfileCounterMode::Maximum
                                                           : ProfileCounterMode::Latest);
        return JS_UNDEFINED;
    }
    if (!s->world)
        return JS_ThrowTypeError(c, "world API unavailable in metadata context");
    auto &world = *s->world;
    if (op == "engineBuildInfo") {
        const auto info = BuildIdentityJson();
        return JS_ParseJSON(c, info.data(), info.size(), "engine build identity");
    }
    if (op == "signalSubscribe" || op == "signalUnsubscribe" || op == "signalEmit" ||
        op == "signalSend" || op == "signalStats")
        return s->SignalNative(op, argc, argv);
    if (op == "scriptFaults") {
        auto out = JS_NewArray(c);
        unsigned n = 0;
        // Bounded, detached snapshots of currently faulted slots, not stale
        // diagnostics from destroyed/reloaded instances.
        const auto &records = s->diagnostics;
        std::set<std::pair<EntityId, uint64_t>> seen;
        for (auto it = records.rbegin(); it != records.rend() && n < 128; ++it) {
            const auto &d = *it;
            if (world.Scripts()->InspectPhase(d.entity, d.slot) != ScriptInstancePhase::Faulted ||
                !seen.emplace(d.entity, d.slot).second)
                continue;
            auto v = JS_NewObject(c);
            JS_SetPropertyStr(c, v, "entity", JS_NewString(c, std::to_string(d.entity).c_str()));
            JS_SetPropertyStr(c, v, "slot", JS_NewString(c, std::to_string(d.slot).c_str()));
            for (const auto &field : std::vector<std::pair<const char *, std::string>>{
                     {"asset", d.asset}, {"callback", d.callback}, {"message", d.message}}) {
                auto text = field.second;
                if (text.size() > 4096) {
                    size_t end = 4096;
                    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
                        --end;
                    text.resize(end);
                }
                JS_SetPropertyStr(c, v, field.first, JS_NewString(c, text.c_str()));
            }
            JS_SetPropertyStr(c, v, "truncated", JS_NewBool(c, d.message.size() > 4096));
            JS_SetPropertyUint32(c, out, n++, v);
        }
        return out;
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
    if (op == "scriptState") {
        auto slot = String(c, arg(2));
        uint64_t slotId = 0;
        try {
            slotId = std::stoull(slot);
        } catch (...) {
            return JS_ThrowTypeError(c, "invalid slot");
        }
        auto it = s->instances.find({id, slotId});
        if (it == s->instances.end() || it->second.fault)
            return JS_NULL;
        auto value = JS_GetPropertyStr(c, it->second.value, "state");
        std::string text, error;
        bool ok = s->Json(value, text, error);
        JS_FreeValue(c, value);
        if (!ok)
            return JS_ThrowTypeError(c, "state: %s", error.c_str());
        return JS_ParseJSON(c, text.data(), text.size(), "state snapshot");
    }
    return JS_ThrowTypeError(c, "unknown runtime operation");
}

ScriptSystem::ScriptSystem(RuntimeWorld *world, const AssetDatabase *assets)
    : m(std::make_unique<Impl>(world, assets)) {}
ScriptSystem::~ScriptSystem() = default;
void ScriptSystem::SetBudget(unsigned n) { m->budget = std::max(1u, n); }
const std::vector<ScriptDiagnostic> &ScriptSystem::Diagnostics() const { return m->diagnostics; }
void ScriptSystem::Stop() {
    JUDAS_PROFILE_SCOPE("JavaScript destroy");
    if (m->debug && m->debug->inside)
        throw std::logic_error("Stop requires a callback boundary");
    JS_SetExecutionHook(m->rt, nullptr, nullptr);
    m->Stop();
}
std::optional<std::vector<SceneObjectId>>
ScriptSystem::DeclaredReferences(SceneObjectId id, const SceneScriptSlot &slot) const {
    auto it = m->instances.find({id, slot.id});
    if (it == m->instances.end() || it->second.slot.asset != slot.asset ||
        it->second.slot.properties != slot.properties)
        return {};
    return it->second.fault ? std::vector<SceneObjectId>{} : it->second.entityReferences;
}
void ScriptSystem::Synchronize(const std::vector<SceneObject> &objects) {
    JUDAS_PROFILE_SCOPE("Script synchronization");
    JUDAS_PROFILE_COUNTER("Script instances", double(m->instances.size()),
                          ProfileCounterMode::Latest);
    m->CheckThread();
    std::set<std::pair<SceneObjectId, uint64_t>> alive;
    for (const auto &object : objects)
        for (const auto &slot : object.scripts)
            if (slot.enabled) {
                auto key = std::make_pair(object.id, slot.id);
                alive.insert(key);
                if (auto existing = m->instances.find(key); existing != m->instances.end()) {
                    if (existing->second.slot.asset == slot.asset &&
                        existing->second.slot.properties == slot.properties) {
                        existing->second.order = &slot - object.scripts.data();
                        continue;
                    }
                    // Replaced authored slot data is a new script instance, never stale
                    // metadata.
                    m->Callback(existing->second, "destroy");
                    if (m->world && m->world->SceneControl())
                        if (auto *stream = m->world->SceneControl()->StreamingIfLoaded())
                            stream->ReleaseRequester(object.id, slot.id);
                    if (m->world && m->world->UIIfLoaded())
                        m->world->UI().RemoveSlotOwner(object.id, slot.id);
                    JS_FreeValue(m->ctx, existing->second.value);
                    m->instances.erase(existing);
                }
                Impl::Instance i;
                i.generation = m->nextGeneration++;
                i.order = &slot - object.scripts.data();
                i.entity = object.id;
                i.slot = slot;
                m->currentOwner = object.id;
                m->currentSlot = slot.id;
                m->polls = 0;
                auto ns = m->Namespace(slot.asset);
                if (JS_IsException(ns)) {
                    m->Error(i, "module");
                    m->instances.emplace(key, std::move(i));
                    continue;
                }
                auto schema = JS_GetPropertyStr(m->ctx, ns, "properties");
                std::string schemaText = "{}", schemaError;
                if (!JS_IsUndefined(schema))
                    m->Json(schema, schemaText, schemaError);
                JS_FreeValue(m->ctx, schema);
                std::vector<ScriptProperty> fields;
                if (!schemaError.empty() ||
                    !ReadProperties(schemaText, slot.properties, fields, schemaError)) {
                    JS_FreeValue(m->ctx, ns);
                    JS_ThrowTypeError(m->ctx, "properties: %s", schemaError.c_str());
                    m->Error(i, "properties");
                    m->instances.emplace(key, std::move(i));
                    continue;
                }
                for (const auto &field : fields)
                    if (field.type == "entity" && field.text != "0")
                        i.entityReferences.push_back(std::stoull(field.text));
                auto ctor = JS_GetPropertyStr(m->ctx, ns, "default");
                JS_FreeValue(m->ctx, ns);
                auto propertyText = WriteProperties(fields);
                auto props = JS_ParseJSON(m->ctx, propertyText.data(), propertyText.size(),
                                          "authored properties");
                auto initialState = m->world->TakeSpawnState(object.id, slot.id);
                auto args = JS_NewObject(m->ctx);
                JS_SetPropertyStr(m->ctx, args, "initialState",
                                  JS_ParseJSON(m->ctx, initialState.data(), initialState.size(),
                                               "prefab initial state"));
                i.loaded = m->resumed.count(key) != 0;
                JS_SetPropertyStr(m->ctx, args, "restored", JS_NewBool(m->ctx, i.loaded));
                JS_SetPropertyStr(m->ctx, args, "properties", props);
                auto libraryNs = m->NamespaceLibrary();
                auto entityFn = JS_GetPropertyStr(m->ctx, libraryNs, "entity");
                for (const auto &field : fields)
                    if (field.type == "entity") {
                        auto id = JS_NewString(m->ctx, field.text.c_str());
                        auto value = JS_Call(m->ctx, entityFn, JS_UNDEFINED, 1, &id);
                        JS_FreeValue(m->ctx, id);
                        JS_SetPropertyStr(m->ctx, props, field.name.c_str(), value);
                    }
                auto id = JS_NewString(m->ctx, std::to_string(object.id).c_str());
                auto self = JS_Call(m->ctx, entityFn, JS_UNDEFINED, 1, &id);
                JS_FreeValue(m->ctx, id);
                JS_FreeValue(m->ctx, entityFn);
                JS_FreeValue(m->ctx, libraryNs);
                JS_SetPropertyStr(m->ctx, args, "entity", self);
                i.value = JS_CallConstructor(m->ctx, ctor, 1, &args);
                JS_FreeValue(m->ctx, ctor);
                JS_FreeValue(m->ctx, args);
                if (JS_IsException(i.value)) {
                    i.value = JS_UNDEFINED;
                    m->Error(i, "construct");
                } else {
                    auto state = m->restored.find(key);
                    std::string text = state == m->restored.end() ? "{}" : state->second;
                    auto existing = JS_GetPropertyStr(m->ctx, i.value, "state");
                    if (state != m->restored.end() || JS_IsUndefined(existing))
                        JS_SetPropertyStr(
                            m->ctx, i.value, "state",
                            JS_ParseJSON(m->ctx, text.data(), text.size(), "saved state"));
                    JS_FreeValue(m->ctx, existing);
                }
                m->instances.emplace(key, std::move(i));
            }
    for (auto it = m->instances.begin(); it != m->instances.end();)
        if (!alive.count(it->first)) {
            m->Callback(it->second, "destroy");
            if (m->world && m->world->SceneControl())
                if (auto *stream = m->world->SceneControl()->StreamingIfLoaded())
                    stream->ReleaseRequester(it->first.first, it->first.second);
            if (m->world && m->world->UIIfLoaded())
                m->world->UI().RemoveSlotOwner(it->first.first, it->first.second);
            JS_FreeValue(m->ctx, it->second.value);
            it = m->instances.erase(it);
        } else
            ++it;
}
void ScriptSystem::RemoveEntities(const std::vector<SceneObjectId> &ids) {
    m->CheckThread();
    std::set<SceneObjectId> gone(ids.begin(), ids.end());
    for (auto it = m->instances.begin(); it != m->instances.end();)
        if (gone.count(it->first.first)) {
            m->Callback(it->second, "destroy");
            if (m->world && m->world->SceneControl())
                if (auto *stream = m->world->SceneControl()->StreamingIfLoaded())
                    stream->ReleaseRequester(it->first.first, it->first.second);
            if (m->world && m->world->UIIfLoaded())
                m->world->UI().RemoveSlotOwner(it->first.first, it->first.second);
            JS_FreeValue(m->ctx, it->second.value);
            it = m->instances.erase(it);
        } else
            ++it;
    for (auto it = m->restored.begin(); it != m->restored.end();)
        if (gone.count(it->first.first))
            it = m->restored.erase(it);
        else
            ++it;
}
void ScriptSystem::Frame(const InputSystem *input, float dt) {
    JUDAS_PROFILE_SCOPE("JavaScript update");
    m->CheckThread();
    m->input = input;
    m->delta = dt;
    m->fixed = false;
    std::vector<std::pair<SceneObjectId, uint64_t>> order;
    for (const auto &entry : m->instances)
        order.push_back(entry.first);
    // Slot order follows authored vector order, not numeric slot identity.
    if (m->world) {
        order.clear();
        for (auto key : m->world->ScriptKeys())
            if (m->instances.count(key))
                order.push_back(key);
    }
    for (auto key : order) {
        auto it = m->instances.find(key);
        if (it == m->instances.end() || (m->world && !m->world->RuntimeDefinition(key.first)))
            continue;
        auto &i = it->second;
        if (!i.started) {
            i.started = true;
            m->Callback(i, i.loaded ? "restore" : "start");
        }
        m->Callback(i, "update");
    }
}
void ScriptSystem::Fixed(const InputSystem *input, float dt) {
    JUDAS_PROFILE_SCOPE("JavaScript fixedUpdate");
    m->CheckThread();
    m->input = input;
    m->delta = dt;
    m->fixed = true;
    auto keys = m->world ? m->world->ScriptKeys() : std::vector<std::pair<EntityId, uint64_t>>{};
    for (auto key : keys) {
        auto it = m->instances.find(key);
        if (it == m->instances.end() || !m->world->RuntimeDefinition(key.first))
            continue;
        auto &i = it->second;
        if (!i.started) {
            i.started = true;
            m->Callback(i, i.loaded ? "restore" : "start");
        }
        m->Callback(i, "fixedUpdate");
    }
}
std::vector<ScriptStateRecord> ScriptSystem::Capture(bool required,
                                                     const std::vector<SceneObjectId> *only) const {
    m->CheckThread();
    std::vector<ScriptStateRecord> result;
    std::set<SceneObjectId> selected;
    if (only)
        selected.insert(only->begin(), only->end());
    for (const auto &entry : m->instances) {
        const auto &i = entry.second;
        if (only && !selected.count(i.entity))
            continue;
        if (m->world && !m->world->RuntimeDefinition(i.entity))
            continue;
        if (i.fault) {
            if (required)
                result.push_back({i.entity, i.slot.id, ""});
            continue;
        }
        auto value = JS_GetPropertyStr(m->ctx, i.value, "state");
        std::string text, error;
        if (m->Json(value, text, error))
            result.push_back({i.entity, i.slot.id, text});
        else {
            result.push_back({i.entity, i.slot.id, ""});
            std::fprintf(stderr, "script state %llu/%llu: %s\n", (unsigned long long)i.entity,
                         (unsigned long long)i.slot.id, error.c_str());
        }
        JS_FreeValue(m->ctx, value);
    }
    return result;
}
bool ScriptSystem::Restore(const std::vector<ScriptStateRecord> &records, std::string &error,
                           bool resume) {
    m->CheckThread();
    for (const auto &r : records)
        if (!ValidateJson(r.json, error))
            return false;
    for (const auto &r : records) {
        auto key = std::make_pair(r.entity, r.slot);
        m->restored[key] = r.json;
        if (resume)
            m->resumed.insert(key);
        if (auto it = m->instances.find(key); it != m->instances.end())
            JS_SetPropertyStr(m->ctx, it->second.value, "state",
                              JS_ParseJSON(m->ctx, r.json.data(), r.json.size(), "saved state"));
    }
    return true;
}

void ScriptSystem::UIFrame(const InputSystem *input, float dt) {
    JUDAS_PROFILE_SCOPE("JavaScript UI update");
    m->CheckThread();
    m->input = input;
    m->delta = dt;
    m->fixed = false;
    m->inUI = true;
    for (auto key : m->world->ScriptKeys()) {
        auto it = m->instances.find(key);
        if (it == m->instances.end() || !m->world->RuntimeDefinition(key.first))
            continue;
        auto &i = it->second;
        if (!i.started) {
            i.started = true;
            m->Callback(i, i.loaded ? "restore" : "start");
        }
        m->Callback(i, "uiUpdate");
    }
    m->inUI = false;
}
void ScriptSystem::UIEvents(const InputSystem *input, float dt) {
    JUDAS_PROFILE_SCOPE("JavaScript UI events");
    m->CheckThread();
    m->input = input;
    m->delta = dt;
    m->fixed = false;
    m->inUI = true;
    if (!m->world->UIIfLoaded()) {
        m->inUI = false;
        return;
    }
    auto events = m->world->UI().TakeEvents();
    for (const auto &event : events)
        for (auto key : m->world->ScriptKeys()) {
            auto it = m->instances.find(key);
            if (it == m->instances.end() || it->second.fault ||
                !m->world->RuntimeDefinition(key.first))
                continue;
            auto &i = it->second;
            Impl::Invocation invocation(*m, i);
            ProfileScope assetScope(m->AssetLabel(i));
            m->polls = 0;
            m->currentOwner = i.entity;
            m->currentSlot = i.slot.id;
            auto fn = JS_GetPropertyStr(m->ctx, i.value, "onUI");
            if (JS_IsException(fn)) {
                m->Error(i, "onUI");
                JS_FreeValue(m->ctx, fn);
                continue;
            }
            if (JS_IsFunction(m->ctx, fn)) {
                auto e = JS_NewObject(m->ctx);
                JS_SetPropertyStr(m->ctx, e, "document",
                                  JS_NewString(m->ctx, event.document.c_str()));
                JS_SetPropertyStr(m->ctx, e, "element",
                                  JS_NewString(m->ctx, event.element.c_str()));
                JS_SetPropertyStr(m->ctx, e, "type", JS_NewString(m->ctx, event.type.c_str()));
                JS_SetPropertyStr(m->ctx, e, "value", JS_NewFloat64(m->ctx, event.value));
                JS_SetPropertyStr(m->ctx, e, "text",
                                  JS_NewStringLen(m->ctx, event.text.data(), event.text.size()));
                JS_SetPropertyStr(m->ctx, e, "item",
                                  JS_NewStringLen(m->ctx, event.item.data(), event.item.size()));
                for (const auto &field : {std::pair<const char *, const std::string &>{
                                              "sourceDocument", event.sourceDocument},
                                          {"sourceElement", event.sourceElement},
                                          {"dragType", event.dragType},
                                          {"targetDocument", event.targetDocument},
                                          {"targetElement", event.targetElement}})
                    JS_SetPropertyStr(
                        m->ctx, e, field.first,
                        JS_NewStringLen(m->ctx, field.second.data(), field.second.size()));
                auto position = JS_NewObject(m->ctx);
                JS_SetPropertyStr(m->ctx, position, "x", JS_NewFloat64(m->ctx, event.position.x));
                JS_SetPropertyStr(m->ctx, position, "y", JS_NewFloat64(m->ctx, event.position.y));
                JS_SetPropertyStr(m->ctx, e, "position", position);
                JS_SetPropertyStr(m->ctx, e, "playerId",
                                  event.player
                                      ? JS_NewString(m->ctx, std::to_string(event.player).c_str())
                                      : JS_NULL);
                auto result = JS_Call(m->ctx, fn, i.value, 1, &e);
                if (JS_IsException(result))
                    m->Error(i, "onUI");
                else if (JS_PromiseState(m->ctx, result) != JS_PROMISE_NOT_A_PROMISE) {
                    JS_ThrowTypeError(m->ctx, "async UI callback unsupported");
                    m->Error(i, "onUI");
                }
                JS_FreeValue(m->ctx, result);
                JS_FreeValue(m->ctx, e);
            }
            JS_FreeValue(m->ctx, fn);
        }
    m->inUI = false;
}

void ScriptSystem::PhysicsEvent(SceneObjectId self, SceneObjectId other,
                                const PhysicsWorld::TouchEvent &event, bool reverse,
                                SceneObjectId selfBody, const std::string &selfJoint,
                                SceneObjectId otherArticulation, const std::string &otherJoint) {
    m->CheckThread();
    m->fixed = true;
    auto *definition = m->world->RuntimeDefinition(self);
    if (!definition)
        return;
    const auto slots = definition->scripts;
    const char *names[2][3] = {{"onCollisionEnter", "onCollisionStay", "onCollisionExit"},
                               {"onTriggerEnter", "onTriggerStay", "onTriggerExit"}};
    const char *name = names[event.sensor ? 1 : 0][static_cast<int>(event.phase)];
    for (const auto &slot : slots) {
        auto *live = m->world->RuntimeDefinition(self);
        if (!live)
            return;
        if (event.phase != PhysicsWorld::TouchPhase::Exit &&
            !m->world->Physics().IsBodyEnabled(reverse ? event.b : event.a))
            return;
        if (std::none_of(live->scripts.begin(), live->scripts.end(),
                         [&](const auto &s) { return s.id == slot.id && s.enabled; }))
            continue;
        auto it = m->instances.find({self, slot.id});
        if (it == m->instances.end() || it->second.fault)
            continue;
        auto &i = it->second;
        if (!i.started) {
            i.started = true;
            m->Callback(i, i.loaded ? "restore" : "start");
        }
        if (!m->world->RuntimeDefinition(self) || i.fault)
            continue;
        Impl::Invocation invocation(*m, i);
        ProfileScope assetScope(m->AssetLabel(i));
        m->polls = 0;
        m->currentOwner = self;
        m->currentSlot = slot.id;
        auto fn = JS_GetPropertyStr(m->ctx, i.value, name);
        if (JS_IsException(fn)) {
            m->Error(i, name);
            JS_FreeValue(m->ctx, fn);
            continue;
        }
        if (JS_IsFunction(m->ctx, fn)) {
            auto e = JS_NewObject(m->ctx);
            auto ns = m->NamespaceLibrary();
            auto entityFn = JS_GetPropertyStr(m->ctx, ns, "entity");
            auto id = JS_NewString(m->ctx, std::to_string(other).c_str());
            auto handle = JS_Call(m->ctx, entityFn, JS_UNDEFINED, 1, &id);
            JS_FreeValue(m->ctx, id);
            JS_FreeValue(m->ctx, entityFn);
            JS_FreeValue(m->ctx, ns);
            JS_SetPropertyStr(m->ctx, e, "other", handle);
            auto makeEntity = [&](SceneObjectId value) {
                auto lib = m->NamespaceLibrary();
                auto fn = JS_GetPropertyStr(m->ctx, lib, "entity");
                auto key = JS_NewString(m->ctx, std::to_string(value).c_str());
                auto result = JS_Call(m->ctx, fn, JS_UNDEFINED, 1, &key);
                JS_FreeValue(m->ctx, key);
                JS_FreeValue(m->ctx, fn);
                JS_FreeValue(m->ctx, lib);
                return result;
            };
            JS_SetPropertyStr(m->ctx, e, "selfBody", makeEntity(selfBody ? selfBody : self));
            JS_SetPropertyStr(m->ctx, e, "selfJoint",
                              selfJoint.empty() ? JS_NULL
                                                : JS_NewString(m->ctx, selfJoint.c_str()));
            JS_SetPropertyStr(m->ctx, e, "otherArticulation", makeEntity(otherArticulation));
            JS_SetPropertyStr(m->ctx, e, "otherJoint",
                              otherJoint.empty() ? JS_NULL
                                                 : JS_NewString(m->ctx, otherJoint.c_str()));
            JS_SetPropertyStr(m->ctx, e, "point", Vec(m->ctx, event.point));
            JS_SetPropertyStr(m->ctx, e, "normal",
                              Vec(m->ctx, reverse ? -event.normal : event.normal));
            JS_SetPropertyStr(
                m->ctx, e, "relativeVelocity",
                Vec(m->ctx, reverse ? -event.relativeVelocity : event.relativeVelocity));
            std::string material;
            float friction = 0, restitution = 0;
            m->world->Physics().GetPhysicalMaterial(m->world->RuntimeBody(other), material,
                                                    friction, restitution);
            JS_SetPropertyStr(m->ctx, e, "physicalMaterial",
                              material.empty() ? JS_NULL : JS_NewString(m->ctx, material.c_str()));
            JS_SetPropertyStr(m->ctx, e, "normalImpulse",
                              event.impulseAvailable ? JS_NewFloat64(m->ctx, event.normalImpulse)
                                                     : JS_NULL);
            auto result = JS_Call(m->ctx, fn, i.value, 1, &e);
            if (JS_IsException(result))
                m->Error(i, name);
            else if (JS_PromiseState(m->ctx, result) != JS_PROMISE_NOT_A_PROMISE) {
                JS_ThrowTypeError(m->ctx, "async contact callbacks unsupported");
                m->Error(i, name);
            }
            JS_FreeValue(m->ctx, result);
            JS_FreeValue(m->ctx, e);
        }
        JS_FreeValue(m->ctx, fn);
    }
}

void ScriptSystem::ShiftOrigin(glm::dvec3 delta) {
    m->CheckThread();
    m->bindingStates.ShiftOrigin();
    m->viewOrigin = glm::vec3(glm::dvec3(m->viewOrigin) - delta);
    const auto vector = [&](glm::dvec3 p) {
        auto value = JS_NewObject(m->ctx);
        JS_SetPropertyStr(m->ctx, value, "x", JS_NewFloat64(m->ctx, p.x));
        JS_SetPropertyStr(m->ctx, value, "y", JS_NewFloat64(m->ctx, p.y));
        JS_SetPropertyStr(m->ctx, value, "z", JS_NewFloat64(m->ctx, p.z));
        return value;
    };
    const bool fixed = m->fixed;
    m->fixed = false;
    // The chart is completely published before notification. This callback is
    // for explicitly owned script caches; arbitrary JSON is never guessed to
    // be a position. Physics intent remains confined to fixedUpdate.
    for (auto key : m->world->ScriptKeys()) {
        auto it = m->instances.find(key);
        if (it == m->instances.end() || !it->second.started || it->second.fault ||
            !m->world->RuntimeDefinition(key.first))
            continue;
        auto &i = it->second;
        Impl::Invocation invocation(*m, i);
        m->polls = 0;
        auto fn = JS_GetPropertyStr(m->ctx, i.value, "onOriginShift");
        if (JS_IsException(fn))
            m->Error(i, "onOriginShift");
        else if (JS_IsFunction(m->ctx, fn)) {
            auto event = JS_NewObject(m->ctx);
            JS_SetPropertyStr(m->ctx, event, "delta", vector(delta));
            JS_SetPropertyStr(m->ctx, event, "origin", vector(m->world->Settings().worldOrigin));
            JS_SetPropertyStr(m->ctx, event, "revision",
                              JS_NewFloat64(m->ctx, double(m->world->OriginRevision())));
            auto result = JS_Call(m->ctx, fn, i.value, 1, &event);
            if (JS_IsException(result))
                m->Error(i, "onOriginShift");
            else if (JS_PromiseState(m->ctx, result) != JS_PROMISE_NOT_A_PROMISE) {
                JS_ThrowTypeError(m->ctx, "async origin callbacks unsupported");
                m->Error(i, "onOriginShift");
            }
            JS_FreeValue(m->ctx, result);
            JS_FreeValue(m->ctx, event);
        }
        JS_FreeValue(m->ctx, fn);
    }
    m->fixed = fixed;
}
void ScriptSystem::SetView(const glm::mat4 &view) {
    m->CheckThread();
    auto inverse = glm::inverse(view);
    m->viewOrigin = glm::vec3(inverse[3]);
    m->viewDirection = -glm::vec3(inverse[2]);
    m->hasView = true;
}

void ScriptSystem::Presentation(const InputSystem *input, float dt, float alpha) {
    JUDAS_PROFILE_SCOPE("JavaScript presentation");
    m->CheckThread();
    m->input = input;
    m->delta = dt;
    m->fixed = false;
    m->presentationAlpha = std::clamp(alpha, 0.f, 1.f);
    m->inPresentation = true;
    for (auto key : m->world->ScriptKeys()) {
        auto it = m->instances.find(key);
        if (it == m->instances.end() || !m->world->RuntimeDefinition(key.first))
            continue;
        auto &instance = it->second;
        if (!instance.started) {
            instance.started = true;
            m->Callback(instance, instance.loaded ? "restore" : "start");
        }
        m->Callback(instance, "presentationUpdate", true);
    }
    m->inPresentation = false;
}

void ScriptSystem::DrainSignals(const InputSystem *input, float dt, bool ui) {
    m->CheckThread();
    m->input = input;
    m->delta = dt;
    m->fixed = !ui;
    m->SignalDrain(ui);
}

#include "ScriptDeveloperToolsMethods.inc"

ScriptInstancePhase ScriptSystem::InspectPhase(SceneObjectId entity, uint64_t slot) const {
    m->CheckThread();
    const auto found = m->instances.find({entity, slot});
    if (found == m->instances.end())
        return ScriptInstancePhase::Absent;
    const auto &instance = found->second;
    if (instance.fault)
        return ScriptInstancePhase::Faulted;
    if (instance.started)
        return ScriptInstancePhase::Running;
    return instance.loaded ? ScriptInstancePhase::AwaitingRestore
                           : ScriptInstancePhase::AwaitingStart;
}

std::vector<ScriptAIInspection> ScriptSystem::AIInspections() const {
    m->CheckThread();
    return ScriptBindings::InspectAI(m->bindingStates);
}
