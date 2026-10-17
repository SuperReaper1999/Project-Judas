#include "AssetCook.h"
#include "BuildIdentity.h"
#include <cstdio>
#include <cstdlib>
int main(int argc, char **argv) {
  if (HandleBuildIdentityArguments(argc, argv, "cook")) return 0;
  if (argc < 3 || argc > 8) {
    std::fprintf(
        stderr,
        "judas_cook source destination.{judastex|judasmesh} [maxTextureSize] "
        "[lodLevels] [lodMaxError] [textureSemantic:0=color,1=data,2=normal] "
        "[textureMode:0=BC,1=RGBA]\n");
    return 2;
  }
  AssetCookSettings s;
  try {
    if (argc > 3)
      s.maxTextureSize = std::stoul(argv[3]);
    if (argc > 4)
      s.lodLevels = std::stoul(argv[4]);
    if (argc > 5)
      s.lodMaxError = std::stof(argv[5]);
    if (argc > 6)
      s.textureSemantic = std::stoul(argv[6]);
    if (argc > 7)
      s.textureMode = std::stoul(argv[7]);
  } catch (...) {
    std::fprintf(stderr, "Invalid numeric cook setting\n");
    return 2;
  }
  std::string error;
  if (!CookAssetFile(argv[1], argv[2], s, error)) {
    std::fprintf(stderr, "Cook failed: %s\n", error.c_str());
    return 1;
  }
  std::printf("Cooked %s\n", argv[2]);
  return 0;
}
