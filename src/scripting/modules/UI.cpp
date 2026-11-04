#include "../../ScriptBindings.h"
#include "../BindingContext.h"
#include "../BindingRegistry.h"

namespace ScriptBindings {
namespace {
JSValue HandleUI(BindingContext &context, const std::string &op, int argc, JSValueConst *argv) {
    return UI(context.js, op, argc, argv, *context.world, context.assets, context.currentOwner,
              context.currentSlot);
}
} // namespace
void RegisterUIBindings(BindingRegistry &registry) {
    registry.Add("UI",
                 {"uiDiagnostics", "uiFind", "uiQuit", "uiLoad", "uiControl", "uiLayout", "uiGet",
                  "uiSet", "uiUnload"},
                 HandleUI);
}
} // namespace ScriptBindings
