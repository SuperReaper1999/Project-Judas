#pragma once

#include "quickjs.h"
#include <cstdint>
#include <string>

class AssetDatabase;
class RuntimeWorld;

// Internal QuickJS bridges. Lifecycle and callback ownership stay in ScriptSystem.
// UI borrows the live owner/slot fields, preserving nested-coercion observations.
namespace ScriptBindings {
JSValue UI(JSContext *c, const std::string &op, int argc, JSValueConst *argv, RuntimeWorld &world,
           const AssetDatabase *assets, const std::uint64_t &owner, const std::uint64_t &slot);
JSValue Joint(JSContext *c, const std::string &op, int argc, JSValueConst *argv,
              RuntimeWorld &world);
} // namespace ScriptBindings
