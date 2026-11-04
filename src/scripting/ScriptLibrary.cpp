#include "ScriptLibrary.h"
#include "ScriptLibraryIncludes.inc"

const std::string& ScriptBindings::LibrarySource() {
    static const std::string library = [] {
        std::string source;
#include "ScriptLibraryAppend.inc"
        return source;
    }();
    return library;
}
