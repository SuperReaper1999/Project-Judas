#include "PerformanceProfiler.h"
#include <array>
struct Event {unsigned kind=0,node=0,parent=0,depth=0,mode=0;std::uint64_t thread=0,frame=0,start=0,end=0,exclusive=0;double value=0;bool wait=false;};
struct Open {unsigned node=0,parent=0;std::uint64_t sequence=0,epoch=0,start=0,children=0,frame=0;bool wait=false;};
struct Cache {std::uint64_t key=0;unsigned node=0;};
struct Lane {
    Lane() noexcept {}
    std::atomic<bool> claimed{false};std::uint64_t id=0;char name[64]{};
    std::array<Event,ProfileLimits::LaneEvents> events{};
    std::atomic<std::uint64_t> write{0},read{0};
    std::array<Open,ProfileLimits::Depth> stack{};unsigned depth=0;std::uint64_t sequence=0;
    std::array<Cache,8192> cache{};std::atomic<unsigned> open{0};
};

struct Probe {std::array<Lane,ProfileLimits::Threads> lanes{};};
Probe* makeProbe(){return new Probe;}
