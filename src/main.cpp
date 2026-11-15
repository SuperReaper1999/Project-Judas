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
#include <filesystem>
#include <vector>
#include "../tools/benchmarks/WholeEngineBenchmark.h"
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
    if(const char* benchmark=std::getenv("JUDAS_APP_BENCHMARK");benchmark&&std::string(benchmark)=="1"){
        const char* output=std::getenv("JUDAS_BENCHMARK_DIR");
        if(!output||!*output){std::fprintf(stderr,"Missing private Android benchmark output directory\n");return 1;}
        const auto setting=[](const char* key,const char* fallback){const auto* value=std::getenv(key);return value&&*value?value:fallback;};
        std::vector<std::string> arguments={"android-frame-capture","--project",(std::filesystem::current_path()/"game.judasproj").string(),"--out",output,
            "--seconds",setting("JUDAS_BENCHMARK_SECONDS","20"),"--warmup-seconds",setting("JUDAS_BENCHMARK_WARMUP","10"),"--cap","0"};
        if(const auto* key=std::getenv("JUDAS_BENCHMARK_KEY");key&&*key){arguments.push_back("--key");arguments.push_back(key);}
        std::vector<char*> native;for(auto& argument:arguments)native.push_back(argument.data());
        return RunWholeEngineBenchmark(int(native.size()),native.data());
    }
#endif
#ifdef __EMSCRIPTEN__
    return RunBrowserApplication(argc,argv);
#else
    Application app;
    return app.Run(argc, argv);
#endif
}
