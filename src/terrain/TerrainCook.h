#pragma once
#include <string>
// Entry point shared by the existing terrain authoring CLI and editor tools.
// No new executable, importer runtime or terrain-specific asset database.
int TerrainV2AuthorCommand(int argc, char **argv);
