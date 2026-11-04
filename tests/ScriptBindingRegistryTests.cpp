#include "scripting/BindingContext.h"
#include "scripting/BindingRegistry.h"

#include <cstdio>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
int handlerCalls = 0;

void Check(bool passed, const char *label) {
    ++checks;
    failures += !passed;
    std::printf("%s %s\n", passed ? "PASS" : "FAIL", label);
}

template <class Operation> bool Throws(Operation operation) {
    try {
        operation();
    } catch (const std::exception &) {
        return true;
    }
    return false;
}

JSValue LocalOperation(ScriptBindings::BindingContext &, const std::string &, int, JSValueConst *) {
    ++handlerCalls;
    return JS_UNDEFINED;
}

class RuntimeStub final : public ScriptBindings::RuntimeServices {
  public:
    bool Json(JSValueConst, std::string &, std::string &) const override { return false; }
    JSValue RuntimeOperation(const std::string &, int, JSValueConst *) override {
        return JS_UNDEFINED;
    }
};

struct Session {
    JSRuntime *runtime = JS_NewRuntime();
    JSContext *js = JS_NewContext(runtime);
    RuntimeStub services;
    ScriptBindings::BindingStates states;
    const InputSystem *input = nullptr;
    bool fixed = false, inUI = false, inPresentation = false, hasView = false;
    bool stopping = false, retiring = false, animation = false;
    float delta = 0, alpha = 1;
    glm::vec3 origin{0}, direction{0};
    std::uint64_t owner = 0, slot = 0;
    ScriptBindings::BindingContext context{js,    nullptr,  nullptr,  services,  states,
                                           input, fixed,    delta,    inUI,      inPresentation,
                                           alpha, hasView,  origin,   direction, owner,
                                           slot,  stopping, retiring, animation};
    ~Session() {
        states.Clear();
        JS_FreeContext(js);
        JS_FreeRuntime(runtime);
    }
    bool TakeException() {
        JSValue exception = JS_GetException(js);
        const bool present = !JS_IsNull(exception);
        JS_FreeValue(js, exception);
        return present;
    }

};

class RecordingState : public ScriptBindings::BindingModuleState {
  public:
    RecordingState(std::vector<std::string> &output, std::string value, unsigned &count)
        : trace(output), name(std::move(value)), destructions(count) {}
    ~RecordingState() override { ++destructions; }
    void ReleaseClaims(std::uint64_t, std::uint64_t) override {
        trace.push_back(name + ":release");
    }
    std::string Retire(std::uint64_t, std::uint64_t) override {
        trace.push_back(name + ":retire");
        return name;
    }
    void ShiftOrigin() override { trace.push_back(name + ":shift"); }

  private:
    std::vector<std::string> &trace;
    std::string name;
    unsigned &destructions;
};

class ReentrantState : public ScriptBindings::BindingModuleState {
  public:
    explicit ReentrantState(ScriptBindings::BindingStates &value) : states(value) {}
    std::string Retire(std::uint64_t owner, std::uint64_t slot) override {
        ++calls;
        if (owner == 1)
            states.Retire(2, slot);
        clearRejected = Throws([&] { states.Clear(); });
        creationRejected = Throws([&] { states.GetOrCreate<ReentrantState>("new", states); });
        return {};
    }
    unsigned calls = 0;
    bool clearRejected = false, creationRejected = false;

  private:
    ScriptBindings::BindingStates &states;
};
} // namespace

int main() {
    using namespace ScriptBindings;
    BindingRegistry registry;
    registry.Add("Local", {"localRead", "localWrite"}, LocalOperation, false);
    Check(registry.Has("localRead") && registry.Entries().at("localRead").module == "Local",
          "module-local registration exposes its own operations and owner");
    Check(Throws([&] { registry.Add("Other", {"new", "localRead"}, LocalOperation); }) &&
              !registry.Has("new") && registry.Entries().size() == 2,
          "duplicate operation rejects the entire registration without prefix publication");
    Check(Throws([&] { registry.Add("Other", {"same", "same"}, LocalOperation); }) &&
              !registry.Has("same"),
          "duplicate names within one module are rejected");
    Check(Throws([&] { registry.Add("", {"x"}, LocalOperation); }) &&
              Throws([&] { registry.Add("Other", {""}, LocalOperation); }) &&
              Throws([&] { registry.Add("Other", {"x"}, nullptr); }) &&
              Throws([&] { registry.Add("Other", {}, LocalOperation); }),
          "empty module, operation, handler and operation list are rejected");
    {
        Session session;
        auto value = registry.Dispatch(session.context, "localRead", 0, nullptr);
        Check(JS_IsUndefined(value) && handlerCalls == 1 && session.states.Empty(),
              "registered handler runs exactly once without allocating session state");
        JS_FreeValue(session.js, value);
        value = registry.Dispatch(session.context, "missing", 0, nullptr);
        Check(JS_IsException(value) && session.TakeException() && handlerCalls == 1,
              "unknown operation raises an exception without running a handler");
        registry.Add("World", {"needsWorld"}, LocalOperation);
        value = registry.Dispatch(session.context, "needsWorld", 0, nullptr);
        Check(JS_IsException(value) && session.TakeException() && handlerCalls == 1,
              "world-dependent operation refuses a metadata context");
    }
    {
        BindingStates states;
        std::vector<std::string> trace;
        unsigned destructions = 0;
        Check(states.Empty() && !states.Find("A"), "optional native module state starts absent");
        auto &first = states.GetOrCreate<RecordingState>("A", trace, "A", destructions);
        Check(&first == &states.GetOrCreate<RecordingState>("A", trace, "ignored", destructions),
              "repeated access retains the same state instance");
        states.GetOrCreate<RecordingState>("B", trace, "B", destructions);
        Check(Throws([&] { states.GetOrCreate<ReentrantState>("A", states); }),
              "a module cannot reuse a state identity with another native type");
        states.ReleaseClaims(1, 1);
        const auto errors = states.Retire(1, 1);
        states.ShiftOrigin();
        Check(trace == std::vector<std::string>{"A:release", "B:release", "A:retire", "B:retire",
                                                "A:shift", "B:shift"} &&
                  errors == "A\nB",
              "lifecycle hooks preserve stable module order and report every cleanup error");
        states.Clear();
        Check(states.Empty() && destructions == 2,
              "explicit runtime shutdown destroys every native state once");
    }
    {
        BindingStates states;
        auto &state = states.GetOrCreate<ReentrantState>("A", states);
        states.Retire(1, 1);
        Check(state.calls == 2 && state.clearRejected && state.creationRejected,
              "nested slot retirement is safe while state destruction and insertion are refused");
        states.Clear();
        Check(states.Empty(), "lifecycle guard releases after nested retirement");
    }
    const auto &installed = BindingRegistry::Default();
    Check(installed.Has("particleInfo") && installed.Has("navConfigure") &&
              installed.Has("audioSettings") && installed.Has("scriptState"),
          "catalogue installs real native modules through the shared registry");
    std::printf("SUMMARY %d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
