#pragma once
#include <string>
namespace JudasParticles::GpuDetail {
std::string ResetShader();
std::string ClearShader();
std::string ShiftShader();
std::string SpawnShader();
std::string SimulationShader(bool collision);
std::string FinalizeShader();
std::string ViewKeysShader();
std::string SortLocalShader();
std::string SortMergeShader();
std::string GatherShader();
}
