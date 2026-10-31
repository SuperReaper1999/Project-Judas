#pragma once
// Isolated Linux diagnostic executable only; never compiled into the engine.
namespace FluidLaunchDiagnostic {
extern bool suppress;
struct Counts { unsigned forceCalls=0, torqueCalls=0, reactionRows=0, swimSamples=0; };
extern Counts counts;
}
