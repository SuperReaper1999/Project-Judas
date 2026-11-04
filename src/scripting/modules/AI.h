#pragma once
#include <vector>
struct ScriptAIInspection;
namespace ScriptBindings {
class BindingStates;
std::vector<ScriptAIInspection> InspectAI(const BindingStates &);
} // namespace ScriptBindings
