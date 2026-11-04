#pragma once

#include <string>

namespace ScriptBindings {
// The public judas module is assembled once per session from compiled-in parts.
// No project file or build-tree JavaScript is read to install the native API.
const std::string& LibrarySource();
} // namespace ScriptBindings
