#include "Application.h"
#include "BuildIdentity.h"
#include "PlatformServices.h"
#include <cstdio>
#include <string>
#ifdef __EMSCRIPTEN__
int RunBrowserApplication(int argc,char** argv);
#endif
#ifdef __ANDROID__
#include <SDL2/SDL_main.h>
#include "../platform/android/AndroidLog.h"
#include <cstdlib>
int RunBrowserApplication(int argc,char** argv);
#endif

int main(int argc, char** argv) {
    if (HandleBuildIdentityArguments(argc, argv, "runtime")) return 0;
#ifdef __ANDROID__
    AndroidLogBridge logBridge;
    if(argc!=3||std::string(argv[1])!="--android-content"||chdir(argv[2])!=0){
        std::fprintf(stderr,"Missing prepared Android content directory\n");return 1;
    }
    setenv("JUDAS_ANDROID_CONTENT",argv[2],1);argc=1;
#endif
#ifdef __EMSCRIPTEN__
    return RunBrowserApplication(argc,argv);
#else
    Application app;
    return app.Run(argc, argv);
#endif
}
