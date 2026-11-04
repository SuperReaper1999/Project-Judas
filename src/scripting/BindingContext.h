#pragma once

#include "quickjs.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <string>

class RuntimeWorld;
class AssetDatabase;
class InputSystem;

namespace ScriptBindings {
class BindingStates;

// The runtime owns execution and serialization. Binding modules borrow these
// services without depending on ScriptSystem's private implementation.
class RuntimeServices {
  public:
    virtual ~RuntimeServices() = default;
    virtual bool Json(JSValueConst, std::string &, std::string &) const = 0;
    virtual JSValue RuntimeOperation(const std::string &, int, JSValueConst *) = 0;
};

// Invocation fields are references deliberately. A JavaScript getter/coercion
// can make a nested native call; modules must observe the live owner and phase.
struct BindingContext {
    JSContext *js;
    RuntimeWorld *world;
    const AssetDatabase *assets;
    RuntimeServices &runtime;
    BindingStates &states;
    const InputSystem *&input;
    bool &fixed;
    float &delta;
    bool &inUI;
    bool &inPresentation;
    float &presentationAlpha;
    bool &hasView;
    glm::vec3 &viewOrigin;
    glm::vec3 &viewDirection;
    std::uint64_t &currentOwner;
    std::uint64_t &currentSlot;
    bool &stopping;
    bool &retiringAI;
    bool &animationControl;

    bool Json(JSValueConst value, std::string &text, std::string &error) {
        return runtime.Json(value, text, error);
    }
};
} // namespace ScriptBindings
