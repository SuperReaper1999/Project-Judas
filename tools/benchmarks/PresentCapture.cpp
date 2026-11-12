// Linux diagnostic interposer: measures the real SDL presentation boundary in
// an unmodified editor or exported runtime. Never linked into shipping tools.
// With the experimental Vulkan backend (JUDAS_GRAPHICS=vulkan) the boundary is
// vkQueuePresentKHR: window_id is 0, present_ms is the duration of that call
// (not equivalent to a blocking GL swap) and swap_interval is the applied
// present mode (0 = IMMEDIATE/MAILBOX, 1 = FIFO).
#include <SDL2/SDL.h>
#ifdef JUDAS_VULKAN
#include <vulkan/vulkan.h>
#include <vector>
#endif
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
bool Uncapped(){const char* uncapped=std::getenv("JUDAS_PRESENT_UNCAPPED");return uncapped&&uncapped[0]=='1'&&uncapped[1]==0;}
void Record(Uint32 id,long long begin,long long end,int interval){
    auto& capture=Output();if(!capture.file)return;
    std::lock_guard<std::mutex> lock(capture.mutex);
    auto& previous=capture.previous[id];
    std::fprintf(capture.file,"%u,%lld,%lld,%.9f,%.9f,%d\n",id,begin,end,
                 previous?double(begin-previous)/1e6:0.,double(end-begin)/1e6,interval);
    previous=begin;
}
#ifdef JUDAS_VULKAN
template<class F> F Next(const char* name){return reinterpret_cast<F>(dlsym(RTLD_NEXT,name));}
std::mutex vulkanMutex;
VkPhysicalDevice vulkanPhysical=VK_NULL_HANDLE;
std::map<VkSwapchainKHR,int> swapIntervals;
#endif
}
#ifdef JUDAS_VULKAN
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physical,const VkDeviceCreateInfo* info,
                                                         const VkAllocationCallbacks* allocator,VkDevice* device){
    static const auto real=Next<PFN_vkCreateDevice>("vkCreateDevice");
    if(!real)return VK_ERROR_INITIALIZATION_FAILED;
    {std::lock_guard<std::mutex> lock(vulkanMutex);vulkanPhysical=physical;}
    return real(physical,info,allocator,device);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device,const VkSwapchainCreateInfoKHR* info,
                                                               const VkAllocationCallbacks* allocator,VkSwapchainKHR* swapchain){
    static const auto real=Next<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR");
    static const auto modes=Next<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>("vkGetPhysicalDeviceSurfacePresentModesKHR");
    if(!real||!info)return VK_ERROR_INITIALIZATION_FAILED;
    VkSwapchainCreateInfoKHR copy=*info;
    VkPhysicalDevice physical;{std::lock_guard<std::mutex> lock(vulkanMutex);physical=vulkanPhysical;}
    if(Uncapped()&&modes&&physical){
        // Same preference as the backend for interval 0, restricted to supported modes.
        std::uint32_t count=0;modes(physical,copy.surface,&count,nullptr);
        std::vector<VkPresentModeKHR> available(count);modes(physical,copy.surface,&count,available.data());
        for(auto mode:available)if(mode==VK_PRESENT_MODE_MAILBOX_KHR)copy.presentMode=mode;
        for(auto mode:available)if(mode==VK_PRESENT_MODE_IMMEDIATE_KHR)copy.presentMode=mode;
    }
    const VkResult result=real(device,&copy,allocator,swapchain);
    if(result==VK_SUCCESS){
        std::lock_guard<std::mutex> lock(vulkanMutex);
        swapIntervals[*swapchain]=copy.presentMode==VK_PRESENT_MODE_IMMEDIATE_KHR||copy.presentMode==VK_PRESENT_MODE_MAILBOX_KHR?0:1;
    }
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue,const VkPresentInfoKHR* info){
    static const auto real=Next<PFN_vkQueuePresentKHR>("vkQueuePresentKHR");
    if(!real){std::fputs("Presentation capture: missing Vulkan present function\n",stderr);std::abort();}
    const auto begin=Now();const VkResult result=real(queue,info);const auto end=Now();
    int interval=-1;
    if(info&&info->swapchainCount){std::lock_guard<std::mutex> lock(vulkanMutex);const auto found=swapIntervals.find(info->pSwapchains[0]);if(found!=swapIntervals.end())interval=found->second;}
    Record(0,begin,end,interval);
    return result;
}
#endif
extern "C" int SDL_GL_SetSwapInterval(int interval){
    static const auto real=reinterpret_cast<int(*)(int)>(dlsym(RTLD_NEXT,"SDL_GL_SetSwapInterval"));
    if(!real)return -1;
    return real(Uncapped()?0:interval);
}
extern "C" void SDL_GL_SwapWindow(SDL_Window* window){
    static const auto real=reinterpret_cast<void(*)(SDL_Window*)>(dlsym(RTLD_NEXT,"SDL_GL_SwapWindow"));
    if(!real){std::fputs("Presentation capture: missing SDL swap function\n",stderr);std::abort();}
    const auto begin=Now();real(window);const auto end=Now();
    Record(SDL_GetWindowID(window),begin,end,SDL_GL_GetSwapInterval());
}
