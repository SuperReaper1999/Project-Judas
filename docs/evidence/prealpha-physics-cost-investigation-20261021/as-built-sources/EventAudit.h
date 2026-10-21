#pragma once
#include <array>
#include <vector>
#include <ostream>
#include <chrono>
#include <ctime>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <limits>
#include <glm/glm.hpp>
namespace event_audit {
enum Stage {Advance, InitialClassification, InitialSchedule, Trial, Legacy, Observations, ClassifySupport,
 WarmCache, PublishMotion, ValidateSupport, DiscoverDirty, Reschedule, Count};
inline constexpr const char* names[Count]={"advance_other","initial_classify","initial_schedule","trial","legacy","observations","classify_support","warm_cache","publish_motion","validate_support","discover_dirty","reschedule"};
struct Timing {std::uint64_t wall=0,cpu=0,calls=0;};
struct Event {std::uint64_t ordinal=0;std::array<unsigned,4> key{};double at=0;
 bool incremental=false,coupled=false;std::uint64_t island=0,changed=0,rows=0;
 std::uint64_t changed_le_1e_7=0,changed_le_1e_5=0,changed_le_one_ulp=0;
 double max_delta=0,min_nonzero_delta=std::numeric_limits<double>::infinity();
 std::uint64_t warm_entries_zeroed=0,warm_entries_removed=0;double warm_normal_discarded=0;
 std::uint64_t wall=0,cpu=0;
};
inline thread_local bool enabled=false;
inline thread_local std::array<Timing,Count> timing{};
inline thread_local std::vector<Event> events;
inline thread_local Event current;
inline thread_local std::uint64_t eventsSeen=0,dropped=0;
inline std::uint64_t CPU(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return std::uint64_t(t.tv_sec)*1000000000ull+t.tv_nsec;}
inline std::uint64_t Wall(){return std::uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());}
struct Scope;inline thread_local Scope* top=nullptr;
struct Scope {bool active=false;Stage kind;Scope* parent=nullptr;std::uint64_t wall=0,cpu=0,childWall=0,childCPU=0;
 explicit Scope(Stage s):active(enabled),kind(s){if(active){parent=top;top=this;wall=Wall();cpu=CPU();}}
 ~Scope(){if(!active)return;const auto dc=CPU()-cpu,dw=Wall()-wall;auto&t=timing[kind];t.wall+=dw-childWall;t.cpu+=dc-childCPU;++t.calls;if(parent){parent->childWall+=dw;parent->childCPU+=dc;}top=parent;}
};
inline void SetEnabled(bool value){enabled=value;}
inline void Reset(){timing={};events.clear();current={};eventsSeen=dropped=0;if(events.capacity()<4096)events.reserve(4096);}
inline std::uint64_t ordered(float x){std::uint32_t u;std::memcpy(&u,&x,4);return u&0x80000000u?~u:u|0x80000000u;}
inline void Begin(std::array<unsigned,4> key,double at,bool coupled){if(!enabled)return;current={};current.ordinal=++eventsSeen;current.key=key;current.at=at;current.coupled=coupled;current.wall=Wall();current.cpu=CPU();}
inline void Delta(glm::vec3 a,glm::vec3 b,glm::vec3 av,glm::vec3 bv){if(!enabled)return;++current.changed;double maximum=0;std::uint64_t ulp=0;for(int k=0;k<3;++k)for(auto p:{std::pair<float,float>{a[k],av[k]},std::pair<float,float>{b[k],bv[k]}}){const auto d=std::abs(double(p.first)-p.second);maximum=std::max(maximum,d);if(d>0)current.min_nonzero_delta=std::min(current.min_nonzero_delta,d);const auto x=ordered(p.first),y=ordered(p.second);ulp=std::max(ulp,x>y?x-y:y-x);}current.max_delta=std::max(current.max_delta,maximum);current.changed_le_1e_7+=maximum<=1e-7;current.changed_le_1e_5+=maximum<=1e-5;current.changed_le_one_ulp+=ulp<=1;}
inline void Discard(float normal,glm::vec3 tangent,bool removing=false){if(!enabled)return;if(normal>0||tangent!=glm::vec3(0)){if(removing)++current.warm_entries_removed;else ++current.warm_entries_zeroed;current.warm_normal_discarded+=normal;}}
inline void End(bool incremental,std::uint64_t island,std::uint64_t rows){if(!enabled)return;current.incremental=incremental;current.island=island;current.rows=rows;current.wall=Wall()-current.wall;current.cpu=CPU()-current.cpu;if(!std::isfinite(current.min_nonzero_delta))current.min_nonzero_delta=0;if(events.size()<events.capacity())events.push_back(current);else ++dropped;}
inline void Header(std::ostream&o){o<<"step,events,dropped";for(const auto*n:names)o<<','<<n<<"_wall_ns,"<<n<<"_cpu_ns,"<<n<<"_calls";o<<'\n';}
inline void EventHeader(std::ostream&o){o<<"step,event,a,b,pa,pb,time,incremental,coupled,island,rows,changed,delta_le_1e_7,delta_le_1e_5,delta_le_one_ulp,max_component_delta,min_nonzero_component_delta,warm_entries_zeroed,warm_entries_removed,warm_normal_discarded,wall_ns,cpu_ns\n";}
inline void WriteStep(std::ostream&o,std::ostream&ev,std::uint64_t step){o<<step<<','<<eventsSeen<<','<<dropped;for(const auto&t:timing)o<<','<<t.wall<<','<<t.cpu<<','<<t.calls;o<<'\n';for(const auto&e:events){ev<<step<<','<<e.ordinal;for(auto k:e.key)ev<<','<<k;ev<<','<<e.at<<','<<e.incremental<<','<<e.coupled<<','<<e.island<<','<<e.rows<<','<<e.changed<<','<<e.changed_le_1e_7<<','<<e.changed_le_1e_5<<','<<e.changed_le_one_ulp<<','<<e.max_delta<<','<<e.min_nonzero_delta<<','<<e.warm_entries_zeroed<<','<<e.warm_entries_removed<<','<<e.warm_normal_discarded<<','<<e.wall<<','<<e.cpu<<'\n';}}
}
