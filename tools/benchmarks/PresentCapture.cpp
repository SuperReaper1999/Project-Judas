// Linux diagnostic interposer: measures the real SDL presentation boundary in
// an unmodified editor or exported runtime. Never linked into shipping tools.
#include <SDL2/SDL.h>
#include <dlfcn.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>

namespace {
using Clock=std::chrono::steady_clock;
long long Now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();}
struct Capture {
    std::mutex mutex;
    std::map<Uint32,long long> previous;
    FILE* file=nullptr;
    char buffer[65536]{};
    Capture(){
        if(const char* path=std::getenv("JUDAS_PRESENT_CAPTURE")){
            // A run must use a fresh file. Never overwrite operator evidence.
            file=std::fopen(path,"wx");
            if(file){std::setvbuf(file,buffer,_IOFBF,sizeof buffer);std::fputs("window_id,begin_ns,end_ns,interval_ms,present_ms,swap_interval\n",file);}
            else std::fprintf(stderr,"Presentation capture could not create fresh output: %s\n",path);
        }
    }
    ~Capture(){if(file)std::fclose(file);}
};
Capture& Output(){static Capture capture;return capture;}
}
extern "C" int SDL_GL_SetSwapInterval(int interval){
    static const auto real=reinterpret_cast<int(*)(int)>(dlsym(RTLD_NEXT,"SDL_GL_SetSwapInterval"));
    if(!real)return -1;
    const char* uncapped=std::getenv("JUDAS_PRESENT_UNCAPPED");
    return real(uncapped&&uncapped[0]=='1'&&uncapped[1]==0?0:interval);
}
extern "C" void SDL_GL_SwapWindow(SDL_Window* window){
    static const auto real=reinterpret_cast<void(*)(SDL_Window*)>(dlsym(RTLD_NEXT,"SDL_GL_SwapWindow"));
    if(!real){std::fputs("Presentation capture: missing SDL swap function\n",stderr);std::abort();}
    const auto begin=Now();real(window);const auto end=Now();
    auto& capture=Output();if(!capture.file)return;
    const auto id=SDL_GetWindowID(window);
    std::lock_guard<std::mutex> lock(capture.mutex);
    auto& previous=capture.previous[id];
    std::fprintf(capture.file,"%u,%lld,%lld,%.9f,%.9f,%d\n",id,begin,end,
                 previous?double(begin-previous)/1e6:0.,double(end-begin)/1e6,SDL_GL_GetSwapInterval());
    previous=begin;
}
