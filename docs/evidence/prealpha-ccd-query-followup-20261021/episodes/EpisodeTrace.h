#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
namespace episode_trace {
using Clock=std::chrono::steady_clock;
inline std::atomic<unsigned> step{0},event{0};
inline unsigned counter=0;
inline thread_local unsigned depth=0;
inline bool Wanted(){auto s=step.load(std::memory_order_relaxed);return s==153||s==461;}
struct Output {
 std::mutex mutex; FILE* queries=nullptr;FILE* events=nullptr;
 Output(){if(auto p=std::getenv("JUDAS_CCD_EPISODE_DIR")){std::string root(p);queries=std::fopen((root+"/queries.csv").c_str(),"w");events=std::fopen((root+"/events.csv").c_str(),"w");
 if(queries)std::fprintf(queries,"step,event,depth,a,b,start,finish,target,result,reason,us,iterations,samples\n");
 if(events)std::fprintf(events,"step,event,a,b,time,delta_time,incremental,bodies,rows,changed,dirty,queries,search_iterations,observation_rows_scanned,warm_rows_scanned,support_rows_scanned,discovery_ms,solve_ms,query_cpu_ms,observation_ms,warm_ms,total_us,kinetic_before,kinetic_after\n");}}
 ~Output(){if(queries)std::fclose(queries);if(events)std::fclose(events);}
};
inline Output& Out(){static Output out;return out;}
struct Query {
 bool enabled=Wanted(); unsigned s=0,e=0,d=0,a,b;double start,finish,target,result=NAN;const char* reason="search";
 const PhysicsWorld::StepStats& stats;std::size_t iterations=0,samples=0;Clock::time_point began;
 Query(unsigned aa,unsigned bb,double st,double fi,double ta,const PhysicsWorld::StepStats& counts):a(aa),b(bb),start(st),finish(fi),target(ta),stats(counts){if(enabled){s=step;e=event;d=depth++;iterations=stats.impactSearchIterations;samples=stats.impactSamplingTests;began=Clock::now();}}
 double Mark(double value){result=value;return value;}
 ~Query(){if(enabled){--depth;const auto us=std::chrono::duration<double,std::micro>(Clock::now()-began).count();auto& out=Out();std::lock_guard<std::mutex> lock(out.mutex);if(out.queries)std::fprintf(out.queries,"%u,%u,%u,%u,%u,%.17g,%.17g,%.17g,%.17g,%s,%.3f,%zu,%zu\n",s,e,d,a,b,start,finish,target,result,reason,us,stats.impactSearchIterations-iterations,stats.impactSamplingTests-samples);}}
};
}
