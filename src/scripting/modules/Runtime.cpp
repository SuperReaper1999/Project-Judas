#include "../BindingContext.h"
#include "../BindingRegistry.h"

namespace ScriptBindings {
namespace {
JSValue HandleRuntime(BindingContext &context, const std::string &op, int argc,
                      JSValueConst *argv) {
    return context.runtime.RuntimeOperation(op, argc, argv);
}
} // namespace
void RegisterRuntimeBindings(BindingRegistry &registry) {
    registry.Add("Runtime", {"modCatalog", "log", "profileScope", "profileCounter"}, HandleRuntime,
                 false);
    registry.Add("Runtime",
                 {"engineBuildInfo", "signalSubscribe", "signalUnsubscribe", "signalEmit",
                  "signalSend", "signalStats", "scriptFaults", "scriptState"},
                 HandleRuntime);
}
} // namespace ScriptBindings
