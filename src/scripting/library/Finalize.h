#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Finalize[] = R"JS(
globalThis.console = console;
)JS";
} // namespace ScriptBindings::LibraryParts
