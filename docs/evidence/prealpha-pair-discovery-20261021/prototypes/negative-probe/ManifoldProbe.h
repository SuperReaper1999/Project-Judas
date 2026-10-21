#pragma once
#include <array>
#include <cstring>
#include <iostream>
#include <map>
namespace manifold_probe {
struct Row { std::array<float,14> pose; bool empty; };
struct Probe {
 uint64_t world=0,step=0,calls=0,negative=0,positive=0,empty=0;
 std::map<std::array<unsigned,4>,Row> last;
 ~Probe(){Flush();}
 void Flush(){if(calls)std::cerr<<"GEOMETRY_PROBE,"<<step<<","<<calls<<","<<negative<<","<<positive<<","<<empty<<"\n";}
 void Record(uint64_t w,uint64_t s,const std::array<unsigned,4>& key,const RigidBody& a,const RigidBody& b,const ContactManifold& m){
  if(world!=w||step!=s){Flush();world=w;step=s;calls=negative=positive=empty=0;last.clear();}
  std::array<float,14> pose{a.position.x,a.position.y,a.position.z,a.orientation.w,a.orientation.x,a.orientation.y,a.orientation.z,b.position.x,b.position.y,b.position.z,b.orientation.w,b.orientation.x,b.orientation.y,b.orientation.z};
  ++calls;empty+=!m.count&&!m.uncertain;
  auto found=last.find(key);
  if(found!=last.end()&&std::memcmp(found->second.pose.data(),pose.data(),sizeof(pose))==0){negative+=found->second.empty;positive+=!found->second.empty;}
  if(found!=last.end()||last.size()<16384)last[key]={pose,!m.count&&!m.uncertain};
 }
};
inline thread_local Probe probe;
}
