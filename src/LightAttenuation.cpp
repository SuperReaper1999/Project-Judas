#include "LightAttenuation.h"

#include <algorithm>
#include <cmath>

float ComputeDistanceAttenuation(float distance, float range) {
    const float safeRange = std::max(range, 1.0e-4f);
    const float rangeFraction = std::clamp(distance / safeRange, 0.0f, 1.0f);
    const float rangeFraction4 = rangeFraction * rangeFraction * rangeFraction * rangeFraction;
    const float windowed = std::clamp(1.0f - rangeFraction4, 0.0f, 1.0f);
    return (windowed * windowed) / (distance * distance + 1.0f);
}

float ComputeSpotConeFactor(float cosAngle, float innerCos, float outerCos) {
    if (outerCos >= innerCos) return cosAngle >= innerCos ? 1.0f : 0.0f;  // degenerate cone: no smoothing possible
    const float t = std::clamp((cosAngle - outerCos) / (innerCos - outerCos), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);  // GLSL smoothstep's own Hermite formula
}
