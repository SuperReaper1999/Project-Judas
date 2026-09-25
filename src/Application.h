#pragma once

// Milestone 28: the `judas` runtime's orchestration — resolve options,
// bring up the engine host, load the selected scene file, instantiate it,
// begin a GameSession, and run either the interactive loop or the scripted
// test harness over it. Owns no scene content and no engine feature; see
// docs/ARCHITECTURE.md, "Milestone 28, Application responsibility split."
class Application {
public:
    // Runs the application to completion. Returns a process exit code
    // (0 on clean shutdown, non-zero if startup failed).
    int Run(int argc, char** argv);
};
