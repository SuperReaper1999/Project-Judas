#include "AuthoringCLI.h"
#include "BuildIdentity.h"
#include "ContentServicesCLI.h"
#include "RecipeCLI.h"
#include "StructuredAuthor.h"

#include <cstdio>
#include <string>

int main(int argc, char **argv) {
    if (HandleBuildIdentityArguments(argc, argv, "scene_author"))
        return 0;
    if (auto result = RunContentServicesCLI(argc, argv); result >= 0)
        return result;
    if (auto result = RunRecipeCLI(argc, argv); result >= 0)
        return result;
    if (auto result = RunAuthoringCLI(argc, argv); result >= 0)
        return result;
    if (argc == 4 && std::string(argv[1]) == "--structured") {
        std::string error;
        if (!WriteStructuredContent(argv[2], argv[3], error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        return 0;
    }
    std::fprintf(stderr,
                 "usage: %s --structured <input.json> <output>\n"
                 "       %s --create|--inspect|--validate|--convert|--edit|--patch ...\n"
                 "See docs/NAMED_AUTHORING.md and docs/SDK_AUTHORING.md.\n",
                 argv[0], argv[0]);
    return argc == 2 && std::string(argv[1]) == "--help" ? 0 : 2;
}
