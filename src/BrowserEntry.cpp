#include "Application.h"
#include <emscripten.h>
#include <emscripten/threading.h>
#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {
struct Launch {
    std::vector<std::string> words;
};
void* RunGame(void* opaque) {
    std::unique_ptr<Launch> launch(static_cast<Launch*>(opaque));
    std::vector<char*> arguments;
    for(auto& word:launch->words)arguments.push_back(word.data());
    int result=1;
    if(chdir("/game")!=0)std::fprintf(stderr,"Missing browser game content\n");
    else {
        Application application;
        result=application.Run(int(arguments.size()),arguments.data());
    }
    // Application and all of its owners have shut down before asking the UI
    // thread to end the page runtime. Do not strand an active engine worker.
    launch.reset();
    emscripten_force_exit(result);
    return nullptr;
}
}

int RunBrowserApplication(int argc,char** argv) {
    auto launch=std::make_unique<Launch>();
    for(int i=0;i<argc;++i)launch->words.emplace_back(argv[i]);
    pthread_attr_t attributes;
    if(pthread_attr_init(&attributes))return 1;
    if(pthread_attr_setstacksize(&attributes,8*1024*1024)||
       emscripten_pthread_attr_settransferredcanvases(&attributes,"#canvas")) {
        pthread_attr_destroy(&attributes);
        std::fprintf(stderr,"Cannot configure browser game worker/canvas\n");return 1;
    }
    pthread_t thread;
    const int error=pthread_create(&thread,&attributes,RunGame,launch.get());
    pthread_attr_destroy(&attributes);
    if(error){std::fprintf(stderr,"Cannot start browser game worker: %d\n",error);return 1;}
    launch.release();
    pthread_detach(thread);
    // Keep browser input, filesystem and audio proxies responsive. The existing
    // engine loop lives on its worker and SDL's Asyncify swap yields frames.
    emscripten_exit_with_live_runtime();
}
