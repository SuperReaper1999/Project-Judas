#include "ParticleImpacts.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace JudasParticles {
bool ParticleImpacts::Cell::operator==(const Cell& b) const {
    return receiver==b.receiver&&revision==b.revision&&child==b.child&&
           x==b.x&&y==b.y&&z==b.z&&normal==b.normal;
}
std::size_t ParticleImpacts::CellHash::operator()(const Cell& c) const {
    std::uint64_t h=1469598103934665603ull;
    for(auto v:{c.receiver,c.revision,std::uint64_t(c.child),std::uint64_t(c.x),
                std::uint64_t(c.y),std::uint64_t(c.z),std::uint64_t(c.normal)})
        h=(h^v)*1099511628211ull;
    return std::size_t(h);
}
namespace {
bool Finite(const ParticleImpact& p) {
    for(int i=0;i<3;++i)if(!std::isfinite(p.position[i])||!std::isfinite(p.localPosition[i])||
        !std::isfinite(p.normal[i])||!std::isfinite(p.localNormal[i])||
        !std::isfinite(p.velocity[i])||!std::isfinite(p.surfaceVelocity[i]))return false;
    for(int i=0;i<4;++i)if(!std::isfinite(p.color[i]))return false;
    return p.count>0&&std::isfinite(p.weight)&&p.weight>=0&&
           std::isfinite(p.size)&&p.size>=0&&std::isfinite(p.age)&&p.age>=0;
}
}
bool ParticleImpacts::Add(const ParticleImpact& impact,const ParticleEmitterSettings& settings) {
    if(settings.impacts==ParticleImpactDelivery::Off)return true;
    if(!Finite(impact))return false;
    const auto overflow=[&](){
        if(settings.impacts!=ParticleImpactDelivery::Cosmetic||
           dropped>std::numeric_limits<std::uint64_t>::max()-impact.count)return false;
        dropped+=impact.count;return true;
    };
    if(sealed)return overflow();
    const auto n=glm::abs(impact.localNormal);
    int axis=n.x>=n.y&&n.x>=n.z?0:n.y>=n.z?1:2;
    std::int64_t coordinate[3];
    for(int i=0;i<3;++i){
        const double q=std::floor(impact.localPosition[i]/settings.impactCellSize);
        if(!std::isfinite(q)||q<=double(std::numeric_limits<std::int64_t>::min())||
           q>=double(std::numeric_limits<std::int64_t>::max()))return false;
        coordinate[i]=std::int64_t(q);
    }
    Cell key{impact.receiver,impact.geometryRevision,impact.childKey,
             coordinate[0],coordinate[1],coordinate[2],axis*2+(impact.localNormal[axis]<0)};
    if(const auto found=cells.find(key);found!=cells.end()){
        auto& original=records[found->second];
        auto p=original;
        if(p.count>std::numeric_limits<std::uint64_t>::max()-impact.count||
           !std::isfinite(p.weight+impact.weight))return false;
        const double total=double(p.count)+double(impact.count),t=double(impact.count)/total;
        p.position=glm::mix(p.position,impact.position,t);
        p.localPosition=glm::mix(p.localPosition,impact.localPosition,t);
        p.normal=glm::mix(p.normal,impact.normal,float(t));
        p.localNormal=glm::mix(p.localNormal,impact.localNormal,float(t));
        p.velocity=glm::mix(p.velocity,impact.velocity,float(t));
        p.surfaceVelocity=glm::mix(p.surfaceVelocity,impact.surfaceVelocity,float(t));
        p.color=glm::mix(p.color,impact.color,float(t));
        p.size=glm::mix(p.size,impact.size,float(t));p.age=glm::mix(p.age,impact.age,float(t));
        p.count+=impact.count;p.weight+=impact.weight;
        // Mean normals are not assumed to remain unit length. Receivers can
        // inspect their magnitude; opposite faces never share a normal bin.
        if(!Finite(p))return false;
        original=p;return true;
    }
    if(records.size()>=settings.maxImpactCells){
        return overflow();
    }
    if(nextSequence==std::numeric_limits<std::uint64_t>::max())return false;
    auto record=impact;record.sequence=nextSequence;
    try {
        records.push_back(record);
        try {
            if(!cells.emplace(key,records.size()-1).second){records.pop_back();return false;}
        } catch(...) {records.pop_back();throw;}
    } catch(const std::bad_alloc&) {return false;}
    ++nextSequence;return true;
}
std::vector<ParticleImpact> ParticleImpacts::Take() {
    std::vector<ParticleImpact> out;out.swap(records);cells.clear();sealed=false;
    for(const auto& p:out)delivered+=p.count;
    return out;
}
void ParticleImpacts::Clear(){records.clear();cells.clear();sealed=false;}
bool ParticleImpacts::Restore(std::vector<ParticleImpact> next,std::uint64_t sequence,std::string& error){
    if(next.size()>65536||!sequence){error="particle impact snapshot capacity/sequence invalid";return false;}
    std::uint64_t prior=0;
    for(const auto& p:next)if(!Finite(p)||p.sequence<=prior||p.sequence>=sequence){
        error="particle impact snapshot record invalid";return false;
    }else prior=p.sequence;
    records=std::move(next);nextSequence=sequence;cells.clear();sealed=!records.empty();error.clear();return true;
}
}
