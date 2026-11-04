#include "../../ScriptBindings.h"
#include "../BindingContext.h"
#include "../BindingRegistry.h"

namespace ScriptBindings {
namespace {
JSValue HandleJoints(BindingContext &context, const std::string &op, int argc, JSValueConst *argv) {
    return Joint(context.js, op, argc, argv, *context.world);
}
} // namespace
void RegisterJointsBindings(BindingRegistry &registry) {
    registry.Add("Joints",
                 {"jointCreate", "jointConfigure", "jointDestroy", "joint", "jointValid",
                  "jointState", "jointSet"},
                 HandleJoints);
}
} // namespace ScriptBindings
