#include "GamePackage.h"
#include "PlatformServices.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

bool ReadGamePackage(const std::string& root, GamePackage& package, std::string& error) {
    std::ifstream file(std::filesystem::path(root) / kGamePackageMarker);
    std::string tag, project, save, trailing;
    int version = 0;
    if (!(file >> tag >> version) || tag != "JudasPackage" || version != 1 ||
        !(file >> tag >> std::quoted(project)) || tag != "project" ||
        !(file >> tag >> std::quoted(save)) || tag != "save-id" || (file >> trailing) ||
        std::filesystem::path(project).filename() != project ||
        std::filesystem::path(project).extension() != ".judasproj" || save.size() != 64 ||
        save.find_first_not_of("0123456789abcdef") != std::string::npos) {
        error = "Invalid or unsupported game package launch record: " + root;
        return false;
    }
    package = {project, save};
    return true;
}

bool PackageSaveDirectory(const GamePackage& package, std::string& directory, std::string& error) {
    const auto root = PlatformUserDataDirectory(error);
    if (root.empty()) return false;
    const auto base = std::filesystem::u8path(root);
    directory = (base / "judas/games" / package.saveId / "Saves").generic_u8string();
    return true;
}
