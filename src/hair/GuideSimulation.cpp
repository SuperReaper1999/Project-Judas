#include "GuideSimulation.h"
#include "MeshData.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace JudasHair {
namespace {
constexpr unsigned coatSlices=36,coatRings=16;
constexpr unsigned coatVertices=(coatSlices+1)*(coatRings+1);
glm::vec3 Unit(glm::vec3 value,glm::vec3 fallback) {
    const float squared=glm::dot(value,value);
    return squared>1e-12f && std::isfinite(squared) ? value/std::sqrt(squared) : fallback;
}
float Random(uint32_t seed) {
    seed^=seed>>16; seed*=0x7feb352du; seed^=seed>>15; seed*=0x846ca68bu; seed^=seed>>16;
    return float(seed>>8)/16777216.f;
}
glm::vec3 Transform(const Frame& frame,glm::vec3 point) {
    return frame.position+frame.rotation*(point*frame.scale);
}
glm::vec3 Side(glm::vec3 direction) {
    return Unit(glm::cross(direction,std::abs(direction.y)>.85f?glm::vec3(1,0,0):glm::vec3(0,1,0)),{0,0,1});
}
glm::vec3 Dome(float fraction,float angle,float coverage) {
    const float y=1-fraction*(1-std::cos(glm::radians(coverage)));
    const float r=std::sqrt(std::max(0.f,1-y*y));
    return {r*std::cos(angle),y,r*std::sin(angle)};
}
glm::vec3 Normal(const Groom& g,glm::vec3 dome) {
    return Unit(dome/glm::max(g.extent,glm::vec3(.001f)),{0,1,0});
}
glm::vec3 Direction(const Groom& g,glm::vec3 dome) {
    const auto normal=Normal(g,dome),flow=Unit(g.flow,{0,1,0});
    const auto tangent=Unit(flow-normal*glm::dot(flow,normal),Side(normal));
    return Unit(glm::mix(normal,tangent,g.comb*.97f),normal);
}
void Coat(const Groom& g,const Frame& frame,std::vector<MeshVertex>& out) {
    // The short-coat shell fills coverage on the fitted patch. Tapered
    // directional fibres only need to supply the broken outer silhouette.
    constexpr float tau=6.28318531f;
    const float coverage=glm::radians(g.coverageDegrees*(.42f+.58f*g.undercoat));
    auto vertex=[&](unsigned ring,unsigned slice) {
        const float u=float(slice)/coatSlices,v=float(ring)/coatRings;
        const float y=std::cos(coverage*v),r=std::sin(coverage*v);
        const auto dome=glm::vec3(r*std::cos(tau*u),y,r*std::sin(tau*u));
        const auto normal=Normal(g,dome);
        const float strandBed=std::min(.025f,g.length*.13f)*g.undercoat;
        const auto local=g.center+g.extent*dome+normal*strandBed;
        auto normalScale=frame.scale;
        for(unsigned axis=0;axis<3;++axis)
            if(std::abs(normalScale[axis])<.00001f)normalScale[axis]=1;
        const auto n=Unit(frame.rotation*(normal/normalScale),frame.rotation*normal);
        const auto tangent=frame.rotation*Direction(g,dome);
        return MeshVertex{Transform(frame,local),n,{u,v},{-1,0},glm::vec4(tangent,1)};
    };
    for(unsigned r=0;r<=coatRings;++r)
        for(unsigned i=0;i<=coatSlices;++i)out.push_back(vertex(r,i));
}

glm::vec3 Lay(const Groom& g,glm::vec3 direction,float fraction) {
    return Unit(glm::mix(direction,Unit(g.flow,direction),g.comb*.57f*fraction*fraction),direction);
}

void Collide(glm::vec3& p,glm::vec3 a,glm::vec3 b,float radius,glm::vec3 fallback) {
    const auto d=b-a;
    const float d2=glm::dot(d,d);
    const auto center=a+d*(d2>1e-12f?std::clamp(glm::dot(p-a,d)/d2,0.f,1.f):0);
    const auto separation=p-center;
    if(glm::dot(separation,separation)<radius*radius) p=center+radius*Unit(separation,fallback);
}
}
void Reset(Guide& guide,const Groom& g,const Frame& frame) {
    const auto root=Transform(frame,guide.root);
    guide.points[0]=guide.previous[0]=root;
    for(unsigned i=1;i<=g.segments;++i) {
        const auto tangent=frame.rotation*Lay(g,guide.direction,(i-.5f)/g.segments);
        guide.points[i]=guide.previous[i]=guide.points[i-1]+tangent*(guide.length/g.segments);
    }
}
void Seed(Guides& state,const Groom& g,const Frame& frame) {
    state.frame=frame;
    state.strands.resize(g.guides);
    constexpr float goldenAngle=2.39996323f;
    const float phase=6.2831853f*Random(g.seed);
    for(unsigned i=0;i<g.guides;++i) {
        auto& guide=state.strands[i];
        const auto dome=Dome((i+.5f)/g.guides,goldenAngle*i+phase,g.coverageDegrees);
        guide.root=g.center+g.extent*dome;
        guide.direction=Direction(g,dome);
        guide.length=g.length*(1-g.variation*Random(g.seed+i+1));
        Reset(guide,g,frame);
    }
    state.fibres.resize(g.strands);
    const float patchOffset=Random(g.seed+127);
    for(unsigned i=0;i<g.strands;++i) {
        auto& fibre=state.fibres[i];
        // Use a two-dimensional sequence with independent coordinates.
        // Using the same golden-ratio increment for both coordinates puts
        // the fibres on one winding line. Stable prefixes retain coverage at LOD.
        const float fraction=std::fmod(patchOffset+(i+.5f)*.754877666f,1.f);
        const float angle=phase+6.2831853f*std::fmod((i+.5f)*.569840291f,1.f);
        const auto dome=Dome(fraction,angle,g.coverageDegrees);
        fibre.root=g.center+g.extent*dome;
        fibre.direction=Direction(g,dome);
        fibre.pigment=.05f+.9f*Random(g.seed+4*i+31);
        fibre.lengthScale=1-g.variation*.55f*Random(g.seed+4*i+23);
        glm::vec3 nearest(1e20f);
        for(unsigned j=0;j<g.guides;++j) {
            const auto delta=(state.strands[j].root-fibre.root)/glm::max(g.extent,glm::vec3(.001f));
            const float distance=glm::dot(delta,delta);
            for(unsigned slot=0;slot<3;++slot) {
                if(distance>=nearest[slot])continue;
                for(unsigned move=2;move>slot;--move) {
                    nearest[move]=nearest[move-1];
                    fibre.guides[move]=fibre.guides[move-1];
                }
                nearest[slot]=distance;
                fibre.guides[slot]=j;
                break;
            }
        }
        fibre.weights=1.f/(nearest+glm::vec3(.035f));
        fibre.weights/=fibre.weights.x+fibre.weights.y+fibre.weights.z;
    }
}
void Step(Guides& state,const Groom& g,const Frame& frame,glm::vec3 gravity,glm::vec3 wind,
          float dt,const SweepTips& sweep) {
    if(state.strands.empty()) Seed(state,g,frame);
    const float resetDistance=std::max(.7f,g.length*2);
    const bool teleport=glm::length(frame.position-state.frame.position)>resetDistance ||
        std::abs(glm::dot(state.frame.rotation,frame.rotation))<.38f;
    state.frame=frame;
    if(teleport) {
        for(auto& guide:state.strands) Reset(guide,g,frame);
        return;
    }
    const auto accel=glm::clamp(gravity+wind*g.wind,glm::vec3(-80),glm::vec3(80))*dt*dt;
    std::array<glm::vec3,4> a{},b{};
    for(size_t i=0;i<g.colliders.size();++i) {
        a[i]=Transform(frame,g.colliders[i].start);
        b[i]=Transform(frame,g.colliders[i].end);
    }
    for(auto& guide:state.strands) {
        const auto root=Transform(frame,guide.root), rest=frame.rotation*guide.direction;
        const float segment=guide.length/g.segments;
        guide.previous[0]=guide.points[0];
        guide.points[0]=root;
        for(unsigned i=1;i<=g.segments;++i) {
            const auto p=guide.points[i], v=(p-guide.previous[i])*(1-g.damping);
            const float travel=glm::length(v);
            guide.points[i]=p+(travel>segment?v*(segment/travel):v)+accel;
            guide.previous[i]=p;
        }
        for(unsigned iteration=0;iteration<4;++iteration) {
            for(unsigned i=1;i<=g.segments;++i) {
                const auto parent=guide.points[i-1];
                auto& point=guide.points[i];
                const float stiffness=g.stiffness*(i==1?.38f:.075f);
                const auto target=frame.rotation*Lay(g,guide.direction,(i-.5f)/g.segments);
                const auto bend=Unit(glm::mix(Unit(point-parent,target),target,stiffness),target);
                point=parent+bend*segment;
                for(size_t c=0;c<g.colliders.size();++c) Collide(point,a[c],b[c],g.colliders[c].radius,rest);
                // Prioritize a bounded segment if the authored collider encloses the root.
                const auto offset=point-parent;
                if(glm::dot(offset,offset)>segment*segment) point=parent+Unit(offset,rest)*segment;
            }
        }
        if(sweep) {
            auto& tip=guide.points[g.segments];
            sweep(guide.previous[g.segments],tip);
            const auto parent=guide.points[g.segments-1],offset=tip-parent;
            if(glm::dot(offset,offset)>segment*segment) tip=parent+Unit(offset,rest)*segment;
        }
    }
}
void Shift(Guides& state,glm::dvec3 delta) {
    const auto move=[&](glm::vec3 p){return glm::vec3(glm::dvec3(p)-delta);};
    state.frame.position=move(state.frame.position);
    for(auto& guide:state.strands)
        for(unsigned i=0;i<guide.points.size();++i) {
            guide.points[i]=move(guide.points[i]);
            guide.previous[i]=move(guide.previous[i]);
        }
}
unsigned VisualStrands(const Groom& g,float distance,float quality) {
    if(distance>g.lodEnd) return 0;
    const float fraction=std::clamp((distance-g.lodStart)/std::max(.01f,g.lodEnd-g.lodStart),0.f,1.f);
    const float density=std::clamp(quality,0.f,1.f)*(1-.8f*fraction);
    const auto count=unsigned(std::ceil(g.strands*density));
    return std::min(g.strands,((count+3)/4)*4);
}
void BuildVertices(const Guides& state,const Groom& g,const Frame& frame,float alpha,
                   unsigned count,std::vector<MeshVertex>& out) {
    out.clear();
    if(state.strands.empty()) return;
    count=std::min(count,unsigned(state.fibres.size()));
    out.reserve(size_t(count)*(g.segments+1)*4+(g.undercoat>0?coatVertices:0));
    if(g.undercoat>0 && count) Coat(g,frame,out);
    std::array<std::array<glm::vec3,9>,32> guideMotion;
    if(g.simulated)
        for(size_t i=0;i<state.strands.size();++i) {
            const auto& guide=state.strands[i];
            const auto root=glm::mix(guide.previous[0],guide.points[0],alpha);
            for(unsigned j=1;j<=g.segments;++j)
                guideMotion[i][j]=glm::mix(guide.previous[j],guide.points[j],alpha)-root;
        }
    const auto extent=glm::max(g.extent,glm::vec3(.001f));
    const float bed=std::min(.025f,g.length*.13f)*g.undercoat;
    const bool combOnShell=!g.simulated&&g.undercoat>0;
    const auto shellExtent=extent+glm::vec3(bed+.0007f);
    auto safeScale=frame.scale;
    for(unsigned axis=0;axis<3;++axis)
        if(std::abs(safeScale[axis])<.00001f)safeScale[axis]=1;
    const auto outerPoint=[&](glm::vec3 p) {
        const auto local=glm::conjugate(frame.rotation)*(p-frame.position)/safeScale;
        const auto radius=(local-g.center)/shellExtent;
        const float length=glm::length(radius);
        if(length>.00001f&&length<1)return Transform(frame,g.center+radius*(shellExtent/length));
        return p;
    };
    for(unsigned i=0;i<count;++i) {
        const auto& fibre=state.fibres[i];
        const auto surfaceNormal=Unit((fibre.root-g.center)/(extent*extent),fibre.direction);
        auto root=Transform(frame,fibre.root+surfaceNormal*(bed*.7f));
        // A combed short fibre may bend into the fitted shell. Keep its
        // rendered nodes outside the bed to avoid clipped latitude bands.
        if(combOnShell)root=outerPoint(root);
        const auto rest=frame.rotation*fibre.direction;
        const auto side=Side(rest);
        glm::vec3 clumpRoot(0);
        float length=0;
        for(unsigned i=0;i<3;++i) {
            const auto& guide=state.strands[fibre.guides[i]];
            clumpRoot+=guide.root*fibre.weights[i];
            length+=guide.length*fibre.weights[i];
        }
        const auto gather=frame.rotation*((clumpRoot-fibre.root)*frame.scale);
        std::array<glm::vec3,9> points;
        points[0]=root;
        auto local=glm::vec3(0);
        for(unsigned j=1;j<=g.segments;++j) {
            const float along=float(j)/g.segments;
            local+=Lay(g,fibre.direction,(j-.5f)/g.segments)*(length/g.segments);
            auto spine=frame.rotation*local;
            if(g.simulated) {
                glm::vec3 motion(0);
                for(unsigned i=0;i<3;++i) {
                    motion+=guideMotion[fibre.guides[i]][j]*fibre.weights[i];
                }
                const float blend=std::min(1.f,along*2.3f);
                spine=glm::mix(spine,motion,blend*blend*(3-2*blend));
            }
            const float wave=std::sin(6.2831853f*(along*1.35f+fibre.pigment))*
                             (g.wave*along*length*.105f);
            points[j]=root+spine*fibre.lengthScale+gather*(g.clump*along*along*.46f)+side*wave;
            if(combOnShell)points[j]=outerPoint(points[j]);
        }
        std::array<glm::vec3,9> tangent, axis;
        auto previousSide=side;
        for(unsigned j=0;j<=g.segments;++j) {
            tangent[j]=Unit(points[std::min(j+1,g.segments)]-points[j?j-1:0],rest);
            axis[j]=Unit(previousSide-tangent[j]*glm::dot(previousSide,tangent[j]),Side(tangent[j]));
            previousSide=axis[j];
        }
        for(unsigned ribbon=0;ribbon<2;++ribbon) {
            const auto emit=[&](unsigned j,float sign) {
                const auto widthAxis=ribbon?glm::cross(tangent[j],axis[j]):axis[j];
                const float along=float(j)/g.segments;
                const float width=g.width*(1-.93f*along*along)*.5f;
                const auto normal=glm::cross(widthAxis,tangent[j]);
                out.push_back({points[j]+widthAxis*(sign*width),normal,{fibre.pigment,along},
                               {0,0},glm::vec4(tangent[j],1)});
            };
            for(unsigned j=0;j<=g.segments;++j) {
                emit(j,-1);
                emit(j,1);
            }
        }
    }
}
void BuildIndices(const Groom& g,unsigned count,std::vector<uint32_t>& out) {
    out.clear();
    if(!count)return;
    const unsigned surface=g.undercoat>0?coatVertices:0;
    out.reserve((surface?coatRings*coatSlices*6:0)+size_t(count)*g.segments*12);
    if(surface)
        for(unsigned r=0;r<coatRings;++r)
            for(unsigned i=0;i<coatSlices;++i) {
                const unsigned a=r*(coatSlices+1)+i,c=a+coatSlices+1;
                if(r)out.insert(out.end(),{a,a+1,c});
                out.insert(out.end(),{a+1,c+1,c});
            }
    for(unsigned i=0;i<count*2;++i) {
        const unsigned base=surface+i*(g.segments+1)*2;
        for(unsigned j=0;j<g.segments;++j) {
            const unsigned a=base+j*2;
            out.insert(out.end(),{a,a+1,a+2,a+2,a+1,a+3});
        }
    }
}
}
