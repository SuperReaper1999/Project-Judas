#pragma once
#include <string>
// Entry point shared by the existing terrain authoring CLI and editor tools.
// No new executable, importer runtime or terrain-specific asset database.
int TerrainV2AuthorCommand(int argc, char **argv);
// Shared V1/V2 policy: published mod inputs and project-escaping links are
// never authoring destinations. Standalone caller-owned sources remain valid.
bool ValidateTerrainWritePath(const std::string &, std::string &);
