#pragma once

// Owns startup, the main loop, and shutdown.
class Application {
public:
    // Runs the application to completion. Returns a process exit code
    // (0 on clean shutdown, non-zero if startup failed).
    int Run();
};
