#pragma once
#include <string>
struct WaterGpuControls {bool passed=false;std::string details;};
class Renderer;
WaterGpuControls RunWaterVisualGpuControls(Renderer&,const std::string& captureDirectory={});
