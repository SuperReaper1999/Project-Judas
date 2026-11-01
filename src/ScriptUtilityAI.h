#pragma once
#include "ScriptSystem.h"
#include "quickjs.h"
#include <memory>

// Session-local ownership bridge. Lazily created only by scripts using AI.
class ScriptUtilityAI {
public:
    explicit ScriptUtilityAI(JSContext* context);
    ~ScriptUtilityAI();
    JSValue Dispatch(const std::string& operation, int argc, JSValueConst* argv,
                     SceneObjectId owner, std::uint64_t slot);
    // Erase native ownership before invoking project cleanup, making nested
    // disposal safe. Exceptions are consumed and returned for diagnostics.
    std::string Retire(SceneObjectId owner, std::uint64_t slot);
    std::vector<ScriptAIInspection> Inspect() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
