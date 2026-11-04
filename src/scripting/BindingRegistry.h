#pragma once

#include "quickjs.h"
#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace ScriptBindings {

struct BindingContext;
using BindingHandler = JSValue (*)(BindingContext&, const std::string&, int, JSValueConst*);

// Immutable after registration. Dispatch performs one lookup and never creates
// module state or rebuilds the operation table.
class BindingRegistry {
public:
    struct Entry {
        std::string module;
        BindingHandler handler;
        bool requiresWorld;
    };
    using OperationMap = std::map<std::string, Entry, std::less<>>;

    void Add(const std::string& module, std::initializer_list<const char*> operations,
             BindingHandler handler, bool requiresWorld = true);
    JSValue Dispatch(BindingContext&, const std::string& operation,
                     int argc, JSValueConst* argv) const;
    bool Has(const std::string& operation) const;
    const OperationMap& Entries() const { return operations; }
    static const BindingRegistry& Default();

private:
    OperationMap operations;
};

// Only modules retaining session-local native/JS resources need one of these.
// RuntimeWorld still owns actual gameplay systems and physical state.
class BindingModuleState {
public:
    virtual ~BindingModuleState() = default;
    virtual void ReleaseClaims(std::uint64_t, std::uint64_t) {}
    virtual std::string Retire(std::uint64_t, std::uint64_t) { return {}; }
    virtual void ShiftOrigin() {}
};

class BindingStates {
public:
    BindingModuleState* Find(const std::string& module);
    const BindingModuleState* Find(const std::string& module) const;

    template<class State, class... Args>
    State& GetOrCreate(const std::string& module, Args&&... args) {
        static_assert(std::is_base_of_v<BindingModuleState, State>);
        if (module.empty())
            throw std::invalid_argument("binding state requires a module name");
        if (auto* existing = Find(module)) {
            auto* state = dynamic_cast<State*>(existing);
            if (!state)
                throw std::logic_error("binding state type mismatch: " + module);
            return *state;
        }
        if (lifecycleDepth)
            throw std::logic_error("cannot create binding state during lifecycle cleanup");
        auto state = std::make_unique<State>(std::forward<Args>(args)...);
        auto* result = state.get();
        if (!states.emplace(module, std::move(state)).second)
            throw std::logic_error("binding state created reentrantly: " + module);
        return *result;
    }

    void ReleaseClaims(std::uint64_t owner, std::uint64_t slot);
    std::string Retire(std::uint64_t owner, std::uint64_t slot);
    void ShiftOrigin();
    void Clear();
    bool Empty() const { return states.empty(); }

private:
    // Nested retirement of another script slot is permitted. Destruction or
    // insertion during a hook would change the set being visited and is refused.
    class LifecycleScope {
    public:
        explicit LifecycleScope(BindingStates& value) : owner(value) { ++owner.lifecycleDepth; }
        ~LifecycleScope() { --owner.lifecycleDepth; }
    private:
        BindingStates& owner;
    };
    std::map<std::string, std::unique_ptr<BindingModuleState>, std::less<>> states;
    unsigned lifecycleDepth = 0;
};

} // namespace ScriptBindings
