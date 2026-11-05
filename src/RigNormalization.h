#pragma once
#include "MeshData.h"
#include <string>

struct RigNormalizationReport {
    double meshScale = 1;
    double maximumMatrixError = 0;
    unsigned posesChecked = 0;
};

// Authoring only: changes coordinate frames, never the physical size of a model.
// On failure output is unchanged. Positive, uniform, shear-free rest frames only.
bool NormalizeModelRig(const MeshData &source, MeshData &output,
                       RigNormalizationReport &report, std::string &error);
