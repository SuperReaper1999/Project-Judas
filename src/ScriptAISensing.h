#pragma once
#include "Scene.h"
#include "quickjs.h"
#include <memory>
#include <string>
class RuntimeWorld;

// No JS callbacks retained. Ownership is identical to the surrounding script slot.
class ScriptAISensing {
public:
    explicit ScriptAISensing(JSContext*);
    ~ScriptAISensing();
    JSValue Dispatch(const std::string& op, int argc, JSValueConst* argv,
                     RuntimeWorld&, SceneObjectId owner, std::uint64_t slot);
    void Retire(SceneObjectId owner, std::uint64_t slot);
    void ShiftOrigin();
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
