#pragma once

#include <string>

// Milestone 30: engine-owned data (the UI font) is not project content and
// must not depend on the process's working directory now that projects
// live anywhere. Resolution order for an engine-relative path such as
// "assets/fonts/DejaVuSans.ttf":
//
//   1. $JUDAS_ENGINE_ROOT/<path>
//   2. <executable dir>/<path>, then <executable dir>/../<path>
//      (a build tree beside the repository root)
//   3. <working directory>/<path>
//
// Returns the first existing candidate, or the last candidate tried so the
// caller's error names a concrete path.
std::string ResolveEngineDataPath(const std::string& relativePath);
std::string EngineExecutableDir();
