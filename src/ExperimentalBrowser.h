#pragma once
#include <string>
#include <vector>

// Deliberate per-launch opt-in; never persisted into projects or preferences.
// Remaining arguments retain their exact bytes and order.
bool ParseExperimentalBrowserArguments(int argc,char** argv,bool& enabled,
                                      std::vector<char*>& remaining,std::string& error);
