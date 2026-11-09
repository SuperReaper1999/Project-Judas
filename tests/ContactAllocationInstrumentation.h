#pragma once
// Test-only whole-workload instrumentation. No replacement physics.
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <type_traits>
#include <utility>
namespace allocation_evidence {
struct Counts { std::size_t calls=0, bytes=0, live=0, peak=0; };
#ifdef FTFT4_ALLOC_TRACKING
Counts counts();
#else
inline Counts counts(){return {};}
#endif
using Clock=std::chrono::steady_clock;
struct Point { Clock::time_point time=Clock::now(); Counts memory=counts(); };
struct Span { double ms=0;Counts memory{}; };
inline Span between(const Point&a,const Point&b){return {std::chrono::duration<double,std::milli>(b.time-a.time).count(),{b.memory.calls-a.memory.calls,b.memory.bytes-a.memory.bytes,b.memory.live,b.memory.peak}};}
inline void print(const char* name,const Span& s){std::printf("\"%s\":{\"ms\":%.12g,\"calls\":%zu,\"bytes\":%zu,\"live\":%zu,\"peak\":%zu}",name,s.ms,s.memory.calls,s.memory.bytes,s.memory.live,s.memory.peak);}
inline Span oracle{},exhaustive{},tree{};
struct Scope { Span& out;Point begin{};explicit Scope(Span& s):out(s){}~Scope(){out=between(begin,Point{});} };
template<class T,class=void>struct Cache{static void print(const T&) {std::printf("\"cache_calls\":null,\"cache_bytes\":null");}};
template<class T>struct Cache<T,std::void_t<decltype(std::declval<T>().geometryCacheAllocations)>>{static void print(const T&s){std::printf("\"cache_calls\":%zu,\"cache_bytes\":%zu",s.geometryCacheAllocations,s.geometryCacheBytes);}};
template<class T,class=void>struct Bytes{static void print(const T&) {std::printf(",\"cache_allocated_bytes\":null");}};
template<class T>struct Bytes<T,std::void_t<decltype(std::declval<T>().geometryCacheAllocatedBytes)>>{static void print(const T&s){std::printf(",\"cache_allocated_bytes\":%zu",s.geometryCacheAllocatedBytes);}};
template<class Stats>struct Meter {
 Point begin{},constructionEnd{},stepBegin{},teardownBegin{};Span steps[30]{};Stats statistics[30]{};
 void built(){constructionEnd=Point{};}
 void start(){stepBegin=Point{};}
 void finish(int i,const Stats&s){steps[i]=between(stepBegin,Point{});statistics[i]=s;}
 void teardown(){teardownBegin=Point{};}
 ~Meter(){
  const Point end{};const auto whole=between(begin,end),setup=between(begin,constructionEnd),destroy=between(teardownBegin,end);
  double simulated=0;for(const auto&s:steps)simulated+=s.ms;
  std::printf("{\"kind\":\"partition\",\"heap_instrumented\":%s,", 
#ifdef FTFT4_ALLOC_TRACKING
 "true"
#else
 "false"
#endif
 );print("whole",whole);std::printf(",");print("construction",setup);std::printf(",");print("teardown",destroy);std::printf(",");print("oracle",oracle);std::printf(",");print("exhaustive",exhaustive);std::printf(",");print("tree_check",tree);
  std::printf(",\"all_steps_ms\":%.12g,\"other_instrumentation_ms\":%.12g,\"steps\":[",simulated,whole.ms-setup.ms-destroy.ms-oracle.ms-simulated);
  for(int i=0;i<30;++i){const auto&s=statistics[i];std::printf("%s{\"index\":%d,",i?",":"",i);print("outer",steps[i]);std::printf(",\"engine_ms\":%.12g,\"broad_ms\":%.12g,\"narrow_ms\":%.12g,\"solver_ms\":%.12g,\"pairs\":%zu,\"points\":%zu,\"exact\":%zu,\"unresolved\":%zu,",s.totalMilliseconds,s.broadphaseMilliseconds,s.narrowphaseMilliseconds,s.solverMilliseconds,s.collidingPairs,s.contactPoints,s.geometryExactFallbacks,s.geometryUnresolved);Cache<Stats>::print(s);Bytes<Stats>::print(s);std::printf("}");}
  std::printf("]}\n");
 }
};
}
