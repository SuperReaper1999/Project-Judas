#pragma once

// Private shared VM/session representation. Only runtime-owned lifecycle, metadata
// and event implementations include this; subsystem bindings use BindingContext.
#include "../../third_party/nlohmann/json.hpp"
#include "../AssetDatabase.h"
#include "../NetworkSession.h"
#include "../PerformanceProfiler.h"
#include "../RuntimeWorld.h"
#include "../ScriptSystem.h"
#include "../TextLayout.h"
#include "BindingContext.h"
#include "BindingRegistry.h"
#include "BindingValues.h"
#include "ScriptLibrary.h"
#include "quickjs.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <tuple>

using ScriptBindingValues::Number;
using ScriptBindingValues::ReadVec;
using ScriptBindingValues::String;
using ScriptBindingValues::Vec;

namespace ScriptRuntimeInternal {
extern std::atomic<uint64_t> metadataParserConstructions;
extern std::atomic<uint64_t> metadataReferenceInspections;
std::string Exception(JSContext *context);
} // namespace ScriptRuntimeInternal

using ScriptRuntimeInternal::Exception;
using ScriptRuntimeInternal::metadataParserConstructions;
using ScriptRuntimeInternal::metadataReferenceInspections;

struct ScriptSystem::Impl : ScriptBindings::RuntimeServices {
    std::thread::id thread = std::this_thread::get_id();
    void CheckThread() const {
        if (thread != std::this_thread::get_id())
            throw std::logic_error("ScriptSystem is main/runtime-thread only");
    }
    RuntimeWorld *world;
    const AssetDatabase *assets;
    JSRuntime *rt = nullptr;
    JSContext *ctx = nullptr;
    struct Instance {
        SceneObjectId entity;
        SceneScriptSlot slot;
        std::vector<SceneObjectId> entityReferences;
        JSValue value = JS_UNDEFINED;
        bool started = false, fault = false, loaded = false;
        size_t order = 0;
        uint64_t generation = 0;
        std::uint32_t profileLabel = 0;
        bool profileRegistered = false;
    };
    std::map<std::pair<SceneObjectId, std::uint64_t>, Instance> instances;
    std::map<std::string, JSModuleDef *> modules;
    std::vector<ScriptDiagnostic> diagnostics;
    ScriptBindings::BindingStates bindingStates;
    bool retiringAI = false;
    void RetireAI(SceneObjectId owner, uint64_t slot) {
        if (bindingStates.Empty())
            return;
        const auto previousOwner = currentOwner;
        const auto previousSlot = currentSlot;
        currentOwner = owner;
        currentSlot = slot;
        polls = 0;
        const bool wasRetiring = retiringAI;
        retiringAI = true;
        bindingStates.ReleaseClaims(owner, slot);
        const auto error = bindingStates.Retire(owner, slot);
        retiringAI = wasRetiring;
        currentOwner = previousOwner;
        currentSlot = previousSlot;
        if (!error.empty())
            diagnostics.push_back({owner, slot, "", "AI cleanup", error});
    }
    bool reloaded = false;
    unsigned invocationDepth = 0;
    std::vector<std::string> consoleMessages;
    struct DebugState {
        ScriptDebugHandler handler;
        std::vector<ScriptBreakpoint> points;
        ScriptDebugAction step = ScriptDebugAction::Continue;
        bool pauseNext = false, inside = false;
        int stepDepth = 0, lastLine = -1, lastDepth = -1;
        JSAtom lastFile = JS_ATOM_NULL;
        std::map<JSAtom, std::string> files;
        std::map<std::string, std::string> sources;
        size_t sourceBytes = 0;
        std::map<int, std::pair<JSAtom, int>> linesByDepth;
    };
    std::unique_ptr<DebugState> debug;
#include "../ScriptDeveloperTools.inc"

    const InputSystem *input = nullptr;
    bool fixed = false;
    float delta = 0;
    bool inUI = false;
    bool inPresentation = false;
    float presentationAlpha = 1;
    bool hasView = false;
    glm::vec3 viewOrigin{0}, viewDirection{0};
    unsigned budget = 10000, polls = 0;
    bool stopping = false;
    SceneObjectId currentOwner = 0;
    std::uint64_t currentSlot = 0;
    std::map<std::pair<SceneObjectId, std::uint64_t>, std::string> restored;
    std::set<std::pair<SceneObjectId, std::uint64_t>> resumed;
#include "../ScriptSignals.inc"
    Impl(RuntimeWorld *w, const AssetDatabase *a, bool jsonOnly = false) : world(w), assets(a) {
        rt = JS_NewRuntime();
        JS_SetMemoryLimit(rt, 64 * 1024 * 1024);
        JS_SetMaxStackSize(rt, 512 * 1024);
        ctx = JS_NewContext(rt);
        JS_SetContextOpaque(ctx, this);
        auto globalPrototype = JS_GetGlobalObject(ctx);
        for (auto item : {std::pair<const char *, JSValue *>{"Object", &plainObjectPrototype},
                          {"Array", &plainArrayPrototype}}) {
            auto ctor = JS_GetPropertyStr(ctx, globalPrototype, item.first);
            *item.second = JS_GetPropertyStr(ctx, ctor, "prototype");
            JS_FreeValue(ctx, ctor);
        }
        JS_FreeValue(ctx, globalPrototype);
        JS_SetInterruptHandler(
            rt,
            [](JSRuntime *, void *p) -> int {
                auto *s = static_cast<Impl *>(p);
                return ++s->polls > s->budget;
            },
            this);
        if (jsonOnly) {
            ++metadataParserConstructions;
            return;
        } // JSON metadata never installs/evaluates project or Judas modules.
        JS_SetModuleLoaderFunc(rt, Normalize, Load, this);
        auto global = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global, "__judas", JS_NewCFunction(ctx, Native, "__judas", 1));
        JS_FreeValue(ctx, global);
        auto lib = NamespaceLibrary();
        JS_FreeValue(ctx, lib);
    }
    struct CachedLabel {
        JSAtom atom = JS_ATOM_NULL;
        std::uint32_t id = 0;
    };
    std::array<CachedLabel, 128> profileLabels{};
    unsigned profileLabelCount = 0;
    std::uint32_t CustomLabel(JSValueConst value) {
        auto &p = PerformanceProfiler::Get();
        if (!p.Active())
            return 0;
        JSAtom atom = JS_ValueToAtom(ctx, value);
        if (atom == JS_ATOM_NULL)
            return 0;
        for (unsigned i = 0; i < profileLabelCount; ++i)
            if (profileLabels[i].atom == atom) {
                JS_FreeAtom(ctx, atom);
                return profileLabels[i].id;
            }
        if (profileLabelCount == profileLabels.size()) {
            JS_FreeAtom(ctx, atom);
            return p.Intern("");
        }
        auto id = p.Intern(String(ctx, value));
        profileLabels[profileLabelCount++] = {atom, id};
        return id;
    }
    std::uint32_t AssetLabel(Instance &i) {
        auto &p = PerformanceProfiler::Get();
        if (!p.Active() || !p.ScriptAttribution())
            return 0;
        if (!i.profileRegistered) {
            i.profileRegistered = true;
            auto *record = assets ? assets->Find(i.slot.asset) : nullptr;
            if (record)
                i.profileLabel = p.Intern("Script asset: " + record->relativePath);
        }
        return i.profileLabel;
    }
    ~Impl() {
        JS_SetExecutionHook(rt, nullptr, nullptr);
        Stop();
        if (debug)
            for (auto [atom, name] : debug->files) {
                (void)name;
                JS_FreeAtom(ctx, atom);
            }
        bindingStates.Clear();
        JS_FreeValue(ctx, plainObjectPrototype);
        JS_FreeValue(ctx, plainArrayPrototype);
        for (unsigned i = 0; i < profileLabelCount; ++i)
            JS_FreeAtom(ctx, profileLabels[i].atom);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
    static char *Normalize(JSContext *c, const char *base, const char *name, void *p) {
        auto *s = static_cast<Impl *>(p);
        std::string result;
        if (std::string(name) == "judas")
            result = "judas";
        else if (std::string(name).rfind("mod:", 0) == 0 && s->assets)
            result = s->assets->PublishedModule(base, name);
        else if (std::string(name).rfind("./", 0) == 0 || std::string(name).rfind("../", 0) == 0) {
            result = (std::filesystem::path(base).parent_path() / name)
                         .lexically_normal()
                         .generic_string();
        } else {
            JS_ThrowReferenceError(
                c, "only relative registered project modules and judas are allowed");
            return nullptr;
        }
        if (result != "judas" &&
            (!s->assets || !s->assets->FindByRelativePath(result) ||
             s->assets->FindByRelativePath(result)->type != AssetType::Script)) {
            JS_ThrowReferenceError(c, "unregistered script import: %s", result.c_str());
            return nullptr;
        }
        if (result != "judas" && std::string(base) != "judas" &&
            !s->assets->ModuleImportAllowed(base, result)) {
            JS_ThrowReferenceError(c, "unpublished cross-owner script import: %s", result.c_str());
            return nullptr;
        }
        return js_strdup(c, result.c_str());
    }
    static JSModuleDef *Load(JSContext *, const char *name, void *p) {
        return static_cast<Impl *>(p)->Module(name);
    }
    JSModuleDef *Module(const std::string &path) {
        if (auto it = modules.find(path); it != modules.end())
            return it->second;
        std::string source;
        if (path == "judas")
            source = ScriptBindings::LibrarySource();
        else {
            const auto *record = assets ? assets->FindByRelativePath(path) : nullptr;
            if (!record || record->missing || record->type != AssetType::Script) {
                JS_ThrowReferenceError(ctx, "missing script %s", path.c_str());
                return nullptr;
            }
            std::ifstream file(record->path);
            std::ostringstream out;
            out << file.rdbuf();
            source = out.str();
            if (!file || source.size() > 1024 * 1024) {
                JS_ThrowReferenceError(ctx, "unreadable or oversized script %s", path.c_str());
                return nullptr;
            }
        }
        if (debug && path != "judas" && debug->sourceBytes + source.size() <= 8 * 1024 * 1024) {
            debug->sources[path] = source;
            debug->sourceBytes += source.size();
        }
        auto value = JS_Eval(ctx, source.data(), source.size(), path.c_str(),
                             JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        if (JS_IsException(value))
            return nullptr;
        auto *module = static_cast<JSModuleDef *>(JS_VALUE_GET_PTR(value));
        modules[path] = module;
        JS_FreeValue(ctx, value);
        return module;
    }
    void Error(Instance &i, const char *callback) {
        i.fault = true;
        SignalRetire({i.entity, i.slot.id});
        auto message = Exception(ctx);
        RetireAI(i.entity, i.slot.id);
        diagnostics.push_back({i.entity, i.slot.id, i.slot.asset, callback, message});
        std::fprintf(stderr, "script asset=%s entity=%llu slot=%llu %s: %s\n", i.slot.asset.c_str(),
                     (unsigned long long)i.entity, (unsigned long long)i.slot.id, callback,
                     message.c_str());
    }
    std::set<std::string> evaluated;
    bool Evaluate(JSModuleDef *module, const std::string &path) {
        if (evaluated.count(path))
            return true;
        auto value = JS_MKPTR(JS_TAG_MODULE, module);
        if (JS_ResolveModule(ctx, value) < 0)
            return false;
        auto result = JS_EvalFunction(ctx, JS_DupValue(ctx, value));
        if (JS_IsException(result))
            return false;
        auto state = JS_PromiseState(ctx, result);
        if (state == JS_PROMISE_REJECTED) {
            auto reason = JS_PromiseResult(ctx, result);
            JS_FreeValue(ctx, result);
            JS_Throw(ctx, reason);
            return false;
        }
        if (state == JS_PROMISE_PENDING) {
            JS_FreeValue(ctx, result);
            JS_ThrowTypeError(ctx, "asynchronous module initialization is unsupported");
            return false;
        }
        JS_FreeValue(ctx, result);
        evaluated.insert(path);
        return true;
    }
    JSValue NamespaceLibrary() {
        auto *module = Module("judas");
        if (!module)
            return JS_EXCEPTION;
        if (!Evaluate(module, "judas"))
            return JS_EXCEPTION;
        return JS_GetModuleNamespace(ctx, module);
    }
    JSValue Namespace(const std::string &asset) {
        const auto *record = assets ? assets->Find(asset) : nullptr;
        if (!record || record->type != AssetType::Script)
            return JS_ThrowReferenceError(ctx, "missing script asset %s", asset.c_str());
        auto *module = Module(record->relativePath);
        if (!module)
            return JS_EXCEPTION;
        if (!Evaluate(module, record->relativePath))
            return JS_EXCEPTION;
        return JS_GetModuleNamespace(ctx, module);
    }
    bool Json(JSValueConst value, std::string &text, std::string &error) const override {
        // No coercion/toJSON: plain own data only, bounded and cycle-free.
        size_t nodes = 0;
        std::set<void *> ancestors;
        std::function<bool(JSValueConst, unsigned)> check = [&](JSValueConst v, unsigned depth) {
            if (depth > 16 || ++nodes > 4096)
                return false;
            if (JS_IsNull(v) || JS_IsBool(v) || JS_IsString(v))
                return true;
            if (JS_IsNumber(v)) {
                double d;
                return JS_ToFloat64(ctx, &d, v) == 0 && std::isfinite(d);
            }
            if (!JS_IsObject(v) || JS_IsFunction(ctx, v))
                return false;
            if (JS_IsArray(v)) {
                auto length = JS_GetPropertyStr(ctx, v, "length");
                uint32_t n = 0;
                JS_ToUint32(ctx, &n, length);
                JS_FreeValue(ctx, length);
                if (n > 4096)
                    return false;
            }
            void *ptr = JS_VALUE_GET_PTR(v);
            if (!ancestors.insert(ptr).second)
                return false;
            auto proto = JS_GetPrototype(ctx, v);
            auto global = JS_GetGlobalObject(ctx);
            auto obj = JS_GetPropertyStr(ctx, global, JS_IsArray(v) ? "Array" : "Object");
            auto expected = JS_GetPropertyStr(ctx, obj, "prototype");
            bool plain = JS_IsNull(proto) || JS_VALUE_GET_PTR(proto) == JS_VALUE_GET_PTR(expected);
            JS_FreeValue(ctx, proto);
            JS_FreeValue(ctx, expected);
            JS_FreeValue(ctx, obj);
            JS_FreeValue(ctx, global);
            JSPropertyEnum *props = nullptr;
            uint32_t count = 0;
            bool ok = plain && JS_GetOwnPropertyNames(ctx, &props, &count, v,
                                                      JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK) == 0;
            for (uint32_t i = 0; ok && i < count; ++i) {
                auto key = JS_AtomToValue(ctx, props[i].atom);
                if (JS_IsSymbol(key))
                    ok = false;
                JS_FreeValue(ctx, key);
                JSPropertyDescriptor desc{};
                int result = JS_GetOwnProperty(ctx, &desc, v, props[i].atom);
                ok = result == 1 && JS_IsUndefined(desc.getter) && JS_IsUndefined(desc.setter) &&
                     check(desc.value, depth + 1);
                JS_FreeValue(ctx, desc.value);
                JS_FreeValue(ctx, desc.getter);
                JS_FreeValue(ctx, desc.setter);
            }
            JS_FreePropertyEnum(ctx, props, count);
            ancestors.erase(ptr);
            return ok;
        };
        if (!check(value, 0)) {
            error = "state must be bounded plain JSON data (no accessors/cycles/nonfinite values)";
            return false;
        }
        auto json = JS_JSONStringify(ctx, value, JS_UNDEFINED, JS_UNDEFINED);
        if (JS_IsException(json)) {
            error = Exception(ctx);
            return false;
        }
        text = String(ctx, json);
        JS_FreeValue(ctx, json);
        if (text.size() > 65536) {
            error = "state exceeds 64KiB";
            return false;
        }
        return true;
    }
    bool animationControl = false; // only authoritative fixedUpdate, not
                                   // post-step contact/UI callbacks
    void Callback(Instance &i, const char *name, bool presentation = false) {
        struct AnimationControlScope {
            Impl &s;
            bool previous;
            AnimationControlScope(Impl &v, const char *n) : s(v), previous(v.animationControl) {
                s.animationControl = std::strcmp(n, "fixedUpdate") == 0;
            }
            ~AnimationControlScope() { s.animationControl = previous; }
        } animationScope(*this, name);
        struct AIRetirement {
            Impl &self;
            Instance &instance;
            bool retire, previous;
            AIRetirement(Impl &s, Instance &i, bool r)
                : self(s), instance(i), retire(r), previous(s.retiringAI) {
                if (retire) {
                    self.retiringAI = true;
                    self.bindingStates.ReleaseClaims(instance.entity, instance.slot.id);
                }
            }
            ~AIRetirement() {
                if (retire)
                    self.RetireAI(instance.entity, instance.slot.id);
                self.retiringAI = previous;
            }
        } aiRetirement{*this, i, std::strcmp(name, "destroy") == 0};
        if (std::strcmp(name, "destroy") == 0)
            SignalRetire({i.entity, i.slot.id});
        if (i.fault)
            return;
        struct CallScope {
            Impl &s;
            CallScope(Impl &v) : s(v) {
                ++s.invocationDepth;
                if (s.debug) {
                    s.debug->lastLine = -1;
                    s.debug->linesByDepth.clear();
                }
            }
            ~CallScope() { --s.invocationDepth; }
        } callScope(*this);
        Invocation invocation(*this, i, std::strcmp(name, "destroy") == 0);
        JUDAS_PROFILE_SCOPE("Script lifecycle callback");
        ProfileScope assetScope(AssetLabel(i));
        currentOwner = i.entity;
        currentSlot = i.slot.id;
        polls = 0;
        auto fn = JS_GetPropertyStr(ctx, i.value, name);
        if (JS_IsException(fn)) {
            Error(i, name);
            return;
        }
        if (JS_IsFunction(ctx, fn)) {
            JUDAS_PROFILE_COUNTER("Script callback calls", 1, ProfileCounterMode::Sum);
            JSValue args[] = {JS_NewFloat64(ctx, delta), JS_NewFloat64(ctx, presentationAlpha)};
            auto result = JS_Call(ctx, fn, i.value, presentation ? 2 : 1, args);
            if (JS_IsException(result))
                Error(i, name);
            else if (JS_PromiseState(ctx, result) != JS_PROMISE_NOT_A_PROMISE) {
                JS_ThrowTypeError(ctx, "async gameplay callbacks are unsupported");
                Error(i, name);
            }
            JS_FreeValue(ctx, result);
        }
        JS_FreeValue(ctx, fn);
    }
    void Stop() {
        if (stopping)
            return;
        stopping = true;
        std::vector<Instance *> order;
        for (auto &entry : instances)
            order.push_back(&entry.second);
        std::sort(order.begin(), order.end(), [](auto *a, auto *b) {
            return a->entity != b->entity ? a->entity < b->entity : a->order < b->order;
        });
        for (auto *i : order) {
            Callback(*i, "destroy");
            JS_FreeValue(ctx, i->value);
        }
        instances.clear();
        signals.reset();
    }
    static JSValue Native(JSContext *c, JSValueConst, int argc, JSValueConst *argv);
    JSValue RuntimeOperation(const std::string &, int, JSValueConst *) override;
};
