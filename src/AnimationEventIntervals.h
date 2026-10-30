#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// Shared half-open marker traversal. Marker/Hit types retain their own storage
// contracts: historical skeletal markers are float; native sprite markers double.
// Scan authored markers, never every skipped cycle. Output is all-or-nothing.
template<class Marker>
bool ValidateAnimationMarkerTrack(double duration,const std::vector<Marker>& events,std::string& error){
    if(!std::isfinite(duration)||duration<0){error="invalid event clip duration";return false;}
    if(events.size()>1024){error="at most 1024 events per clip";return false;}
    std::vector<std::string> ids;
    for(const auto& e:events){
        if(e.id.empty()||e.id.size()>128||e.name.empty()||e.name.size()>128||e.data.size()>4096||!std::isfinite(e.time)||e.time<0||e.time>duration||std::find(ids.begin(),ids.end(),e.id)!=ids.end()){
            error="event requires unique id, name, bounded data and time within clip";
            return false;
        }
        ids.push_back(e.id);
    }
    return true;
}
template<class Hit,class Marker>
std::vector<Hit> CollectAnimationMarkers(const std::string& name,double duration,const std::vector<Marker>& events,
    double from,double to,bool loop,bool& overflow,size_t limit=256){
    std::vector<Hit> result;
    overflow=false;
    if(!std::isfinite(from)||!std::isfinite(to)||std::abs(from)>1e9||std::abs(to)>1e9){overflow=true;return result;}
    if(from==to||events.empty())return result;
    const bool reverse=to<from;
    if(!loop||duration<=0){from=std::clamp(from,0.0,duration);to=std::clamp(to,0.0,duration);}
    for(const auto& e:events){
        double first=0,last=0;
        if(loop&&duration>0){
            first=reverse?std::ceil((to-e.time)/duration):std::floor((from-e.time)/duration)+1;
            last=reverse?std::ceil((from-e.time)/duration)-1:std::floor((to-e.time)/duration);
        }else if(!(reverse?(to<=e.time&&e.time<from):(from<e.time&&e.time<=to)))continue;
        if(std::abs(first)>1000000||std::abs(last)>1000000){overflow=true;return {};}
        if(last-first+1>double(limit-result.size())){overflow=true;return {};}
        for(double k=first;k<=last;++k){
            Hit hit;hit.event=e;hit.clip=name;hit.crossing=e.time+(loop?k*duration:0);hit.reverse=reverse;
            result.push_back(std::move(hit));
        }
    }
    std::stable_sort(result.begin(),result.end(),[&](const auto& a,const auto& b){
        if(a.crossing!=b.crossing)return reverse?a.crossing>b.crossing:a.crossing<b.crossing;
        return a.event.id<b.event.id;
    });
    return result;
}
