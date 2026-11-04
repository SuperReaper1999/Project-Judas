#pragma once
struct EditorContext;
struct EditorPlaySession;

// Measured editor diagnostics have their own smoothing state. Collection does
// not advance, publish or otherwise mutate the observed simulation.
class EditorProfilerSnapshot {
  public:
    void Collect(EditorContext &, const EditorPlaySession &, float frameDeltaSeconds);

  private:
    float averageMilliseconds = 16.f;
};
