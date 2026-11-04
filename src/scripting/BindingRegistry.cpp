#include "BindingRegistry.h"
#include "BindingContext.h"
#include <set>

namespace ScriptBindings {

#define JUDAS_SCRIPT_MODULE(registration) void registration(BindingRegistry&);
#include "ScriptModuleRegistrations.inc"
#undef JUDAS_SCRIPT_MODULE

void BindingRegistry::Add(const std::string& module,
                          std::initializer_list<const char*> names,
                          BindingHandler handler, bool requiresWorld) {
    if (module.empty() || !handler || names.size() == 0)
        throw std::invalid_argument("binding registration requires module, operations and handler");
    std::set<std::string> unique;
    for (const char* name : names) {
        if (!name || !*name)
            throw std::invalid_argument("binding operation must have a name");
        if (operations.count(name) || !unique.insert(name).second)
            throw std::logic_error("duplicate binding operation: " + std::string(name));
    }
    // Validate the full declaration first. Ordinary invalid registration cannot
    // publish only the prefix of a module's operation list.
    for (const auto& name : unique)
        operations.emplace(name, Entry{module, handler, requiresWorld});
}

JSValue BindingRegistry::Dispatch(BindingContext& context, const std::string& operation,
                                 int argc, JSValueConst* argv) const {
    const auto found = operations.find(operation);
    if (found == operations.end())
        return JS_ThrowTypeError(context.js, "unknown engine operation: %s", operation.c_str());
    if (found->second.requiresWorld && !context.world)
        return JS_ThrowTypeError(context.js, "world API unavailable in metadata context");
    return found->second.handler(context, operation, argc, argv);
}

bool BindingRegistry::Has(const std::string& operation) const {
    return operations.find(operation) != operations.end();
}

const BindingRegistry& BindingRegistry::Default() {
    static const BindingRegistry registry = [] {
        BindingRegistry result;
#define JUDAS_SCRIPT_MODULE(registration) registration(result);
#include "ScriptModuleRegistrations.inc"
#undef JUDAS_SCRIPT_MODULE
        return result;
    }();
    return registry;
}

BindingModuleState* BindingStates::Find(const std::string& module) {
    const auto found = states.find(module);
    return found == states.end() ? nullptr : found->second.get();
}

const BindingModuleState* BindingStates::Find(const std::string& module) const {
    const auto found = states.find(module);
    return found == states.end() ? nullptr : found->second.get();
}

void BindingStates::ReleaseClaims(std::uint64_t owner, std::uint64_t slot) {
    LifecycleScope scope(*this);
    for (auto& [module, state] : states)
        state->ReleaseClaims(owner, slot);
}

std::string BindingStates::Retire(std::uint64_t owner, std::uint64_t slot) {
    LifecycleScope scope(*this);
    std::string errors;
    for (auto& [module, state] : states) {
        auto error = state->Retire(owner, slot);
        if (!error.empty()) {
            if (!errors.empty())
                errors += '\n';
            errors += error;
        }
    }
    return errors;
}

void BindingStates::ShiftOrigin() {
    LifecycleScope scope(*this);
    for (auto& [module, state] : states)
        state->ShiftOrigin();
}

void BindingStates::Clear() {
    if (lifecycleDepth)
        throw std::logic_error("cannot clear binding states during lifecycle cleanup");
    states.clear();
}

} // namespace ScriptBindings
