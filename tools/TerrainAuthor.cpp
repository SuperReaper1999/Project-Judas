#include "BuildIdentity.h"
#include "terrain/TerrainCook.h"
#include <iostream>
int main(int argc, char **argv) {
    if (HandleBuildIdentityArguments(argc, argv, "terrain_author")) return 0;
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "judas_terrain_author v2-new | v2-heightmap | v2-planet | v2-native | v2-pack | v2-validate\n";
        return 0;
    }
    return TerrainV2AuthorCommand(argc, argv);
}
