#include "Application.h"
#include "PlatformServices.h"
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--build-info") {
        std::printf("Judas runtime %s %s\n", RuntimePlatformName(), JUDAS_RUNTIME_BUILD_CONFIG);
        return 0;
    }
    Application app;
    return app.Run(argc, argv);
}
