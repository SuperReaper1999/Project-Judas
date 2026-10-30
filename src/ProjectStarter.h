#pragma once
#include <string>
class Project;
class Scene;
// Author ordinary assets into a new project, never a hidden runtime demo mode.
bool PopulateVisibleProjectStarter(Project&,Scene&,std::string& error);

bool Populate2DProjectStarter(Project&,Scene&,bool sideView,std::string& error);

bool PopulatePlanarProjectStarter(Project&,Scene&,bool sideView,std::string& error);

// Kinematic, cast-checked cardinal movement; game policy remains editable JS.
bool PopulateGridProjectStarter(Project&,Scene&,std::string& error);

// Preserve existing bindings; add edge-latched cardinal actions for grid taps.
bool ConfigureGridPlayerInput(Project&,std::string& error);
