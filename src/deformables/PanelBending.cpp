#include "PanelBending.h"
#include "SaveArchive.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
namespace {
double Wrap(double a){return std::atan2(std::sin(a),std::cos(a));}
// Plastic flow needs the same signed angle and validity checks, but none of
// the constraint gradients. Keep one formula and specialise only unused work.
template<bool Gradients>
inline bool HingeGeometry(const glm::dvec3* p,double& angle,glm::dvec3* g){
    auto e=p[1]-p[0];double l2=glm::dot(e,e);if(!std::isfinite(l2)||l2<1e-18)return false;
    auto n=glm::cross(e,p[2]-p[0]),m=glm::cross(p[3]-p[0],e);
    double n2=glm::dot(n,n),m2=glm::dot(m,m);if(!std::isfinite(n2)||!std::isfinite(m2)||n2<1e-18||m2<1e-18)return false;
    double length=std::sqrt(l2);auto a=n/std::sqrt(n2),b=m/std::sqrt(m2);
    angle=std::atan2(glm::dot(e/length,glm::cross(a,b)),glm::dot(a,b));
    if constexpr(Gradients){
    g[2]=-length*n/n2;g[3]=-length*m/m2;
    g[0]=-glm::dot(p[1]-p[2],e)/l2*g[2]-glm::dot(p[1]-p[3],e)/l2*g[3];
    g[1]=-glm::dot(p[2]-p[0],e)/l2*g[2]-glm::dot(p[3]-p[0],e)/l2*g[3];
    }
    return true;
}
}
bool PanelBending::AngleGradient(const glm::dvec3* p,double& angle,glm::dvec3* g){
    return HingeGeometry<true>(p,angle,g);
}
std::shared_ptr<const PanelBending::Topology> PanelBending::Prepare(const std::vector<glm::dvec3>& positions,const std::vector<glm::uvec3>& triangles){
    std::map<std::pair<unsigned,unsigned>,std::vector<unsigned>> edges;
    for(auto t:triangles)for(unsigned k=0;k<3;++k)edges[std::minmax(t[k],t[(k+1)%3])].push_back(t[(k+2)%3]);
    auto prepared=std::make_shared<Topology>();
    for(const auto& [edge,opposite]:edges){
        if(opposite.size()>2)throw std::invalid_argument("nonmanifold panel hinge");
        if(opposite.size()!=2)continue;
        RestHinge h;h.nodes={edge.first,edge.second,opposite[0],opposite[1]};
        glm::dvec3 p[4];for(int k=0;k<4;++k)p[k]=positions.at(h.nodes[k]);
        // Ordinary cloth may have geometry below the signed-panel kernel
        // precision envelope. Keep that capability; enabling panel bending
        // explicitly rejects an unavailable prepared topology.
        if(!HingeGeometry<false>(p,h.authored,nullptr))return {};
        prepared->push_back(h);
    }
    return prepared;
}
void PanelBending::Initialize(const std::vector<glm::dvec3>& positions,const std::vector<glm::uvec3>& triangles){
    Initialize(Prepare(positions,triangles));
}
void PanelBending::Initialize(std::shared_ptr<const Topology> prepared){
    if(!prepared||!std::isfinite(stiffness)||stiffness<=0||!std::isfinite(yieldMoment)||yieldMoment<=0||!std::isfinite(hardening)||hardening<0)throw std::invalid_argument("invalid panel bending material/topology");
    std::vector<Hinge> state(prepared->size());
    for(size_t i=0;i<state.size();++i)state[i].rest=prepared->at(i).authored;
    topology=std::move(prepared);hinges=std::move(state);
}
void PanelBending::Reset(){for(size_t i=0;i<hinges.size();++i){auto& h=hinges[i];h.rest=topology->at(i).authored;h.lambda=h.plastic=h.work=0;}}
void PanelBending::BeginSubstep(){for(auto& h:hinges)h.lambda=0;}
namespace {
void ProjectPanelHinge(const PanelBending::RestHinge& authored,PanelBending::Hinge& h,std::vector<glm::dvec3>& positions,const std::vector<double>& inverseMass,double alpha){
    const auto& nodes=authored.nodes;glm::dvec3 p[4],g[4];for(int k=0;k<4;++k)p[k]=positions[nodes[k]];double angle;
    if(!HingeGeometry<true>(p,angle,g))return;
    double den=alpha;for(int k=0;k<4;++k)den+=inverseMass[nodes[k]]*glm::dot(g[k],g[k]);
    double dl=(-Wrap(angle-h.rest)-alpha*h.lambda)/den;h.lambda+=dl;
    for(int k=0;k<4;++k)positions[nodes[k]]+=g[k]*(inverseMass[nodes[k]]*dl);
}
}
void PanelBending::Solve(std::vector<glm::dvec3>& positions,const std::vector<double>& inverseMass,double dt){
    const double alpha=1/(stiffness*dt*dt);
    for(size_t i=0;i<hinges.size();++i)ProjectPanelHinge((*topology)[i],hinges[i],positions,inverseMass,alpha);
}
void PanelBending::Plastic(const std::vector<glm::dvec3>& positions,double){
    for(size_t i=0;i<hinges.size();++i){auto& h=hinges[i];const auto& nodes=(*topology)[i].nodes;glm::dvec3 p[4];for(int k=0;k<4;++k)p[k]=positions[nodes[k]];double angle;
        if(!HingeGeometry<false>(p,angle,nullptr))continue;
        const double strain=Wrap(angle-h.rest),moment=stiffness*strain,capacity=yieldMoment+hardening*h.plastic;
        if(std::abs(moment)<=capacity)continue;
        const double flow=(std::abs(moment)-capacity)/(stiffness+hardening);
        h.rest=Wrap(h.rest+std::copysign(flow,strain));h.plastic+=flow;h.work+=capacity*flow+.5*hardening*flow*flow;
    }
}
void PanelBending::Persist(SaveArchive& a){
    a(stiffness,yieldMoment,hardening);a.Require(std::isfinite(stiffness)&&stiffness>0&&std::isfinite(yieldMoment)&&yieldMoment>0&&std::isfinite(hardening)&&hardening>=0,"invalid saved panel material");
    unsigned count=unsigned(hinges.size());a(count);a.Require(count==hinges.size(),"panel hinge topology mismatch");
    for(auto& h:hinges){a(h.rest,h.plastic,h.work);a.Require(std::isfinite(h.rest)&&std::isfinite(h.plastic)&&h.plastic>=0&&std::isfinite(h.work)&&h.work>=0,"invalid panel plastic state");if(a.reading)h.lambda=0;}
}
