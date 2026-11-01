#include "ScriptUtilityAI.h"
#include "ScriptBindingValues.h"
#include "UtilityAI.h"
#include "PerformanceProfiler.h"
#include <map>
#include <stdexcept>
#include <cmath>

using ScriptBindingValues::String;
struct ScriptUtilityAI::Impl {
    struct Entry {
        SceneObjectId owner;
        std::uint64_t slot;
        std::string name;
        std::unique_ptr<UtilityAgent> agent;
        JSValue retire;
    };
    JSContext* context;
    std::uint64_t next = 1;
    std::map<std::string, Entry> entries;
    explicit Impl(JSContext* c) : context(c) {}
    ~Impl() { for (auto& entry : entries) JS_FreeValue(context, entry.second.retire); }
    std::string Cleanup(Entry& entry) {
        auto result = JS_Call(context, entry.retire, JS_UNDEFINED, 0, nullptr);
        std::string error;
        if (JS_IsException(result)) {
            auto exception = JS_GetException(context);
            error = String(context, exception);
            JS_FreeValue(context, exception);
        }
        JS_FreeValue(context, result);
        JS_FreeValue(context, entry.retire);
        return error;
    }
};
ScriptUtilityAI::ScriptUtilityAI(JSContext* c) : m(std::make_unique<Impl>(c)) {}
ScriptUtilityAI::~ScriptUtilityAI() = default;
JSValue ScriptUtilityAI::Dispatch(const std::string& op, int argc, JSValueConst* argv,
                                  SceneObjectId owner, std::uint64_t slot) {
    auto* c = m->context;
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };
    auto parsed = [&](const std::string& json) { return JS_ParseJSON(c, json.data(), json.size(), "AI state"); };
    try {
        if (!owner || !slot) throw std::invalid_argument("AI agents require an owning script instance");
        if (op == "aiCreate") {
            if (!JS_IsString(arg(1)) || !JS_IsString(arg(2)) || !JS_IsFunction(c, arg(3)))
                throw std::invalid_argument("AI definition, name and cleanup callback required");
            const auto name = String(c, arg(2));
            if (name.empty() || name.size() > 128 || m->entries.size() >= 4096)
                throw std::invalid_argument("AI name or session agent limit exceeded");
            unsigned owned = 0;
            for (const auto& entry : m->entries)
                owned += entry.second.owner == owner && entry.second.slot == slot;
            if (owned >= 16) throw std::invalid_argument("at most 16 AI agents per script slot");
            auto agent = std::make_unique<UtilityAgent>(String(c, arg(1)));
            const auto handle = std::to_string(m->next++);
            const auto callback = JS_DupValue(c, arg(3));
            try { m->entries.emplace(handle, Impl::Entry{owner, slot, name, std::move(agent), callback}); }
            catch (...) { JS_FreeValue(c, callback); throw; }
            return JS_NewString(c, handle.c_str());
        }
        if (!JS_IsString(arg(1))) throw std::invalid_argument("AI agent handle required");
        const auto found = m->entries.find(String(c, arg(1)));
        if (found == m->entries.end() || found->second.owner != owner || found->second.slot != slot)
            throw std::invalid_argument("stale or foreign AI agent");
        auto& agent = *found->second.agent;
        if (op == "aiDue" || op == "aiTick") {
            double dt = 0;
            if (!JS_IsNumber(arg(2)) || JS_ToFloat64(c, &dt, arg(2)) < 0 || !std::isfinite(dt))
                throw std::invalid_argument("finite AI timestep required");
            if (op == "aiDue") return JS_NewBool(c, agent.Due(dt));
            if (!JS_IsString(arg(3))) throw std::invalid_argument("AI candidates JSON required");
            JUDAS_PROFILE_SCOPE("Utility AI decision");
            return parsed(agent.Tick(dt, String(c, arg(3))));
        }
        if (op == "aiInspect") return parsed(agent.Inspect());
        if (op == "aiSnapshot") return parsed(agent.Snapshot());
        if (op == "aiRestore") {
            if (!JS_IsString(arg(2))) throw std::invalid_argument("AI snapshot JSON required");
            agent.Restore(String(c, arg(2)));
            return JS_UNDEFINED;
        }
        if (op == "aiFinish") {
            if (!JS_IsString(arg(2))) throw std::invalid_argument("AI outcome must be a primitive string");
            agent.Finish(String(c, arg(2))); return JS_UNDEFINED;
        }
        if (op == "aiDispose") {
            auto entry = std::move(found->second);
            m->entries.erase(found);
            const auto error = m->Cleanup(entry);
            if (!error.empty()) return JS_ThrowInternalError(c, "AI cleanup: %s", error.c_str());
            return JS_UNDEFINED;
        }
        throw std::invalid_argument("unknown AI op");
    } catch (const std::exception& error) {
        return JS_ThrowTypeError(c, "AI: %s", error.what());
    }
}
std::string ScriptUtilityAI::Retire(SceneObjectId owner, std::uint64_t slot) {
    std::vector<Impl::Entry> retired;
    for (auto it = m->entries.begin(); it != m->entries.end();) {
        if (it->second.owner == owner && it->second.slot == slot) {
            retired.push_back(std::move(it->second));
            it = m->entries.erase(it);
        } else ++it;
    }
    std::string errors;
    for (auto& entry : retired) {
        const auto error = m->Cleanup(entry);
        if (!error.empty()) errors += entry.name + ": " + error + "\n";
    }
    return errors;
}
std::vector<ScriptAIInspection> ScriptUtilityAI::Inspect() const {
    std::vector<ScriptAIInspection> result;
    for (const auto& entry : m->entries) {
        const auto& e = entry.second;
        result.push_back({e.owner, e.slot, e.name, e.agent->Inspect()});
    }
    return result;
}
