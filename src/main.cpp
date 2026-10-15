#include "Application.h"
#include "PlatformServices.h"
#include <cstdio>
#include <string>
#ifdef __ANDROID__
#include <SDL2/SDL_main.h>
#include "../platform/android/AndroidLog.h"
#include <cstdlib>
#include <unistd.h>
#endif

int main(int argc, char** argv) {
#ifdef __ANDROID__
    AndroidLogBridge logBridge;
    if(argc!=3||std::string(argv[1])!="--android-content"||chdir(argv[2])!=0){
        std::fprintf(stderr,"Missing prepared Android content directory\n");return 1;
    }
    setenv("JUDAS_ANDROID_CONTENT",argv[2],1);argc=1;
#endif
    if (argc == 2 && std::string(argv[1]) == "--build-info") {
        std::printf("Judas runtime %s %s\n", RuntimePlatformName(), JUDAS_RUNTIME_BUILD_CONFIG);
        return 0;
    }
    Application app;
    return app.Run(argc, argv);
}
