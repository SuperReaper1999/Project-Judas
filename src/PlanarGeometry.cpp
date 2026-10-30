#include "PlanarGeometry.h"
#include "CollisionShapes.h"
#include "Contacts.h"
#include "PhysicsCastGeometry.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace {
constexpr double pi=3.1415926535897932384626433832795;
double Cross(glm::dvec2 a,glm::dvec2 b){return a.x*b.y-a.y*b.x;}
bool Finite(glm::dvec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::max(std::abs(p.x),std::abs(p.y))<=1e6;}
std::shared_ptr<const PlanarCollider> Checked(PlanarCollider c){std::string error;if(!ValidatePlanar(c,error))throw std::invalid_argument(error);return std::make_shared<const PlanarCollider>(std::move(c));}
}
bool ValidatePlanar(const PlanarCollider& c,std::string& error,bool query){
    auto fail=[&](const char* message){error=message;return false;};
    if(int(c.kind)<0||int(c.kind)>4||!std::isfinite(c.radius)||c.radius<0||c.radius>1e6)return fail("invalid 2D shape kind/radius");
    const auto count=c.vertices.size();
    if(c.oneWay&&(c.kind!=PlanarKind::Chain||count!=2))return fail("one-way collision requires a straight static chain with two vertices");
    if(count==0||count>(c.kind==PlanarKind::Chain?1024u:64u))return fail("2D shape vertex limit exceeded");
    for(auto p:c.vertices)if(!Finite(p))return fail("2D vertices must be finite and bounded");
    if(c.kind==PlanarKind::Circle){if(count!=1||c.vertices[0]!=glm::dvec2(0)||(!query&&c.radius<=0))return fail("2D circle requires positive radius");}
    else if(c.kind==PlanarKind::Capsule){if(count!=2||c.radius<=0||c.vertices[0].x!=0||c.vertices[1].x!=0||c.vertices[0].y!=-c.vertices[1].y||c.vertices[1].y<0)return fail("invalid 2D capsule core");}
    else {
        if(c.radius!=0||count<(c.kind==PlanarKind::Chain?2u:3u))return fail("2D polygon/chain requires unrounded vertices");
        for(size_t i=1;i<count;++i)if(glm::length(c.vertices[i]-c.vertices[i-1])<1e-8)return fail("2D geometry contains a degenerate edge");
        if(c.kind!=PlanarKind::Chain){
            if(glm::length(c.vertices.front()-c.vertices.back())<1e-8)return fail("2D polygon repeats its closing vertex");
            double area=0;for(size_t i=0;i<count;++i)area+=Cross(c.vertices[i],c.vertices[(i+1)%count]);
            if(std::abs(area)<1e-12)return fail("2D polygon area is degenerate");
            for(size_t i=0;i<count;++i)for(size_t j=0;j<count;++j)
                if(j!=i&&j!=(i+1)%count&&Cross(c.vertices[(i+1)%count]-c.vertices[i],c.vertices[j]-c.vertices[i])*area<=0)
                    return fail("2D polygon must be convex and non-self-intersecting");
            double sign=0;
            for(size_t i=0;i<count;++i){double turn=Cross(c.vertices[(i+1)%count]-c.vertices[i],c.vertices[(i+2)%count]-c.vertices[(i+1)%count]);
                if(std::abs(turn)<1e-12||sign*turn<0)return fail("2D polygon must be strictly convex with consistent winding");
                sign=turn;}
        }
    }
    if(c.kind==PlanarKind::Box) {
        if(count!=4||c.vertices[0].x>=0||c.vertices[0].y>=0)return fail("2D box requires four centered rectangle corners");
        const auto h=-c.vertices[0];
        if(c.vertices!=std::vector<glm::dvec2>{{-h.x,-h.y},{h.x,-h.y},{h.x,h.y},{-h.x,h.y}})
            return fail("2D box requires centered axis-aligned corners; use convex for other polygons");
    }
    error.clear();return true;
}
std::shared_ptr<const PlanarCollider> PlanarCircle(double radius,bool lock){return Checked({PlanarKind::Circle,{{0,0}},radius,lock});}
std::shared_ptr<const PlanarCollider> PlanarBox(glm::dvec2 h,bool lock){if(!Finite(h)||h.x<=0||h.y<=0)throw std::invalid_argument("2D box half extents must be positive");return Checked({PlanarKind::Box,{{-h.x,-h.y},{h.x,-h.y},{h.x,h.y},{-h.x,h.y}},0,lock});}
std::shared_ptr<const PlanarCollider> PlanarCapsule(double r,double h,bool lock){if(!std::isfinite(h)||h<0||h>1e6)throw std::invalid_argument("invalid 2D capsule half height");return Checked({PlanarKind::Capsule,{{0,-h},{0,h}},r,lock});}
std::shared_ptr<const PlanarCollider> PlanarConvex(std::vector<glm::dvec2> points,bool lock){return Checked({PlanarKind::Convex,std::move(points),0,lock});}
std::shared_ptr<const PlanarCollider> PlanarChain(std::vector<glm::dvec2> points,bool oneWay){return Checked({PlanarKind::Chain,std::move(points),0,true,oneWay});}
PlanarMass PlanarMassProperties(const PlanarCollider& c){
    std::string error;if(!ValidatePlanar(c,error))throw std::invalid_argument(error);
    PlanarMass result;
    if(c.kind==PlanarKind::Chain)return result;
    if(c.kind==PlanarKind::Circle){const double r=c.radius;result.area=pi*r*r;result.inertia={pi*std::pow(r,4)/4,pi*std::pow(r,4)/4,pi*std::pow(r,4)/2};return result;}
    if(c.kind==PlanarKind::Capsule){const double r=c.radius,h=c.vertices[1].y;
        result.area=4*h*r+pi*r*r;
        result.inertia.x=4*r*h*h*h/3+pi*r*r*h*h+8*h*r*r*r/3+pi*std::pow(r,4)/4;
        result.inertia.y=4*h*r*r*r/3+pi*std::pow(r,4)/4;
        result.inertia.z=result.inertia.x+result.inertia.y;return result;}
    double crossSum=0,xx=0,yy=0,xy=0;glm::dvec2 center{0};
    for(size_t i=0;i<c.vertices.size();++i){auto a=c.vertices[i],b=c.vertices[(i+1)%c.vertices.size()];double d=Cross(a,b);crossSum+=d;center+=(a+b)*d;xx+=d*(a.x*a.x+a.x*b.x+b.x*b.x);yy+=d*(a.y*a.y+a.y*b.y+b.y*b.y);xy+=d*(2*a.x*a.y+a.x*b.y+b.x*a.y+2*b.x*b.y);}
    result.center=center/(3*crossSum);result.area=std::abs(crossSum)/2;
    const double sign=crossSum>0?1:-1;
    result.inertia.x=sign*yy/12-result.area*result.center.y*result.center.y;
    result.inertia.y=sign*xx/12-result.area*result.center.x*result.center.x;
    result.inertia.z=result.inertia.x+result.inertia.y;
    result.product=sign*xy/24-result.area*result.center.x*result.center.y;
    if(!(result.area>1e-12&&result.inertia.x>0&&result.inertia.y>0))throw std::invalid_argument("degenerate 2D lamina mass");
    return result;
}
std::string EncodePlanar(const PlanarCollider& c){std::string error;if(!ValidatePlanar(c,error))throw std::invalid_argument(error);nlohmann::ordered_json j={{"version",1},{"kind",int(c.kind)},{"radius",c.radius},{"lockRotation",c.lockRotation},{"vertices",nlohmann::ordered_json::array()}};for(auto p:c.vertices)j["vertices"].push_back({p.x,p.y});if(c.oneWay){j["version"]=2;j["oneWay"]=true;}return j.dump();}
bool DecodePlanar(const std::string& text,std::shared_ptr<const PlanarCollider>& out,std::string& error){try{
    if(text.size()>128*1024)throw std::invalid_argument("2D descriptor exceeds 128 KiB");
    auto j=nlohmann::json::parse(text);
    const bool version2=j.is_object()&&j.value("version",0)==2;
    if(!j.is_object()||j.size()!=(version2?6u:5u)||(j.at("version")!=1&&!version2)||!j.at("kind").is_number_integer()||!j.at("radius").is_number()||!j.at("lockRotation").is_boolean()||!j.at("vertices").is_array())throw std::invalid_argument("invalid 2D descriptor/version");
    PlanarCollider c;if(version2){if(!j.at("oneWay").is_boolean())throw std::invalid_argument("oneWay must be boolean");c.oneWay=j.at("oneWay").get<bool>();}c.kind=PlanarKind(j.at("kind").get<int>());c.radius=j.at("radius").get<double>();c.lockRotation=j.at("lockRotation").get<bool>();
    if(j["vertices"].size()>1024)throw std::invalid_argument("2D vertex limit");
    for(const auto& p:j["vertices"]){if(!p.is_array()||p.size()!=2||!p[0].is_number()||!p[1].is_number())throw std::invalid_argument("invalid 2D vertex");c.vertices.push_back({p[0].get<double>(),p[1].get<double>()});}
    auto value=Checked(std::move(c));out=std::move(value);error.clear();return true;
}catch(const std::exception& e){error=e.what();return false;}}
std::vector<glm::dvec2> PlanarCore(const Shape& s){
    if(!s.planar)throw std::invalid_argument("missing 2D geometry");
    if(s.planar->kind!=PlanarKind::Chain)return s.planar->vertices;
    const auto index=s.planarSegment;
    if(index<0||size_t(index+1)>=s.planar->vertices.size())throw std::invalid_argument("invalid 2D chain segment");
    return {s.planar->vertices[index],s.planar->vertices[index+1]};
}

ContactManifold PlanarContacts(const Shape& a,const ContactPose& pa,const Shape& b,const ContactPose& pb,double margin,glm::dvec3 translationA){
    ContactManifold result;
    if(a.type!=ShapeType::Planar||b.type!=ShapeType::Planar||!a.planar||!b.planar)return result;
    const auto ra=ContactRotation(pa.orientation),rb=ContactRotation(pb.orientation);
    const auto delta=glm::dvec3(pb.position)-glm::dvec3(pa.position)-translationA;
    const auto normal=ra[2];
    if(std::abs(glm::dot(normal,rb[2]))<1-1e-6||std::abs(glm::dot(normal,delta))>1e-4)return result;
    const auto ca=PlanarCore(a),cb=PlanarCore(b);
    std::vector<glm::dvec2> va,vb;
    for(auto p:ca)va.push_back(glm::dvec2(p)+glm::dvec2(pa.localCenter));
    for(auto p:cb)vb.push_back(glm::dvec2(glm::transpose(ra)*(delta+rb*(glm::dvec3(p,0)+glm::dvec3(pb.localCenter)))));
    std::vector<glm::dvec2> axes;
    const auto add=[&](glm::dvec2 axis){const double length=glm::length(axis);if(length>1e-12)axes.push_back(axis/length);};
    const auto edges=[&](const auto& points){std::vector<std::pair<glm::dvec2,glm::dvec2>> values;
        const auto count=points.size()>2?points.size():points.size()==2?1u:0u;
        for(size_t i=0;i<count;++i){auto p=points[i],q=points[(i+1)%points.size()];values.push_back({p,q});auto d=q-p;add({-d.y,d.x});}return values;};
    const auto ea=edges(va),eb=edges(vb);
    const auto closest=[](glm::dvec2 p,glm::dvec2 x,glm::dvec2 y){auto d=y-x;const double l=glm::dot(d,d);return l>0?x+d*std::clamp(glm::dot(p-x,d)/l,0.,1.):x;};
    // Polygon face normals plus rounded-feature closest directions form the
    // complete rounded-core SAT candidates. No collision thickness is added.
    if(a.planar->radius>0||b.planar->radius>0){
        for(auto p:va)for(auto q:vb)add(p-q);
        for(auto p:va)for(auto [x,y]:eb)add(p-closest(p,x,y));
        for(auto p:vb)for(auto [x,y]:ea)add(closest(p,x,y)-p);
    }
    if(axes.empty())add(vb[0]-va[0]);
    if(axes.empty())add({1,0});
    double gap=-std::numeric_limits<double>::infinity();glm::dvec2 n{1,0};
    for(auto axis:axes){
        double amin=INFINITY,amax=-INFINITY,bmin=INFINITY,bmax=-INFINITY;
        for(auto p:va){const auto d=glm::dot(p,axis);amin=std::min(amin,d);amax=std::max(amax,d);}
        for(auto p:vb){const auto d=glm::dot(p,axis);bmin=std::min(bmin,d);bmax=std::max(bmax,d);}
        const double first=amin-bmax-a.planar->radius-b.planar->radius;
        const double second=bmin-amax-a.planar->radius-b.planar->radius;
        const double candidate=std::max(first,second);
        if(candidate>gap){gap=candidate;n=first>=second?axis:-axis;}
    }
    if(gap>margin)return result;
    // Build contacts from actual incident edges, not only the extreme support
    // vertex. Two slightly tilted penetrating corners are two constraints;
    // dropping the shallower corner makes a resting box alternate pivots.
    const auto emitWitness=[&](glm::dvec2 wa,glm::dvec2 wb,double separation){
        const auto mid=(wa+wb)*.5;const auto worldMid=ra*glm::dvec3(mid,0),worldB=ra*glm::dvec3(wb,0);
        Contact c;c.hit=true;c.preciseNormal=ra*glm::dvec3(n,0);c.normal=glm::vec3(c.preciseNormal);
        c.signedSeparation=separation;c.penetration=float(-separation);c.hasLocalAnchors=true;
        c.localAnchorA=glm::dvec3(mid,0);c.localAnchorB=glm::transpose(rb)*(worldMid-delta);
        c.localWitnessA=glm::dvec3(wa,0);c.localWitnessB=glm::transpose(rb)*(worldB-delta);
        c.point=glm::vec3(glm::dvec3(pa.position)+translationA+worldMid);
        c.separationState=separation>0?SeparationState::Separated:separation<0?SeparationState::Penetrating:SeparationState::Touching;
        result.Add(c);
    };
    if(a.planar->radius==0&&b.planar->radius==0&&va.size()>=2&&vb.size()>=2){
        struct Edge {glm::dvec2 first,last,normal;double alignment=-INFINITY;};
        const auto face=[&](const auto& vertices,glm::dvec2 wanted){
            Edge selected;double winding=0;
            for(size_t i=0;i<vertices.size();++i)winding+=Cross(vertices[i],vertices[(i+1)%vertices.size()]);
            const auto count=vertices.size()==2?1:vertices.size();
            for(size_t i=0;i<count;++i){auto x=vertices[i],y=vertices[(i+1)%vertices.size()],d=y-x;
                auto outward=glm::normalize(glm::dvec2(d.y,-d.x));if(winding<0)outward=-outward;
                if(vertices.size()==2&&glm::dot(outward,wanted)<0)outward=-outward;
                double alignment=glm::dot(outward,wanted);if(alignment>selected.alignment)selected={x,y,outward,alignment};
            }return selected;
        };
        const auto edgeA=face(va,-n),edgeB=face(vb,n);
        const bool referenceA=edgeA.alignment>=edgeB.alignment;
        const auto reference=referenceA?edgeA:edgeB,incident=referenceA?edgeB:edgeA;
        // An SAT normal belongs to a reference face. Near-equal alignment is
        // geometric roundoff only; the selected points keep their own gaps.
        if(reference.alignment>1-1e-10){
            const auto tangent=glm::normalize(reference.last-reference.first);
            double low=glm::dot(tangent,reference.first),high=glm::dot(tangent,reference.last);if(low>high)std::swap(low,high);
            auto x=incident.first,y=incident.last;auto d=y-x;double start=0,end=1;
            const double origin=glm::dot(tangent,x),speed=glm::dot(tangent,d);
            bool intersects=true;
            if(std::abs(speed)<1e-15)intersects=origin>=low&&origin<=high;
            else {double t0=(low-origin)/speed,t1=(high-origin)/speed;if(t0>t1)std::swap(t0,t1);start=std::max(start,t0);end=std::min(end,t1);intersects=start<=end;}
            if(intersects){for(double t:{start,end}){auto witness=x+d*t;double separation=glm::dot(reference.normal,witness-reference.first);
                if(separation<=margin){auto projected=witness-reference.normal*separation;
                    if(referenceA)emitWitness(projected,witness,separation);else emitWitness(witness,projected,separation);}
                if(end-start<1e-10)break;
            }
            if(result.count)return result;
            }
        }
    }
    const glm::dvec2 tangent{-n.y,n.x};
    struct Feature {glm::dvec2 first,last;double lo=0,hi=0;};
    const auto feature=[&](const auto& points,glm::dvec2 outward,double radius){
        Feature f;double extreme=-INFINITY;
        for(auto p:points)extreme=std::max(extreme,glm::dot(outward,p));
        bool first=true;
        for(auto p:points)if(extreme-glm::dot(outward,p)<=1e-10*std::max(1.,std::abs(extreme))){
            p+=outward*radius;const double t=glm::dot(tangent,p);
            if(first){f={p,p,t,t};first=false;}else{if(t<f.lo){f.lo=t;f.first=p;}if(t>f.hi){f.hi=t;f.last=p;}}
        }
        return f;
    };
    const auto fa=feature(va,-n,a.planar->radius),fb=feature(vb,n,b.planar->radius);
    const auto at=[](const Feature& f,double t){return f.hi>f.lo?glm::mix(f.first,f.last,std::clamp((t-f.lo)/(f.hi-f.lo),0.,1.)):f.first;};
    const double lo=std::max(fa.lo,fb.lo),hi=std::min(fa.hi,fb.hi);
    const auto emit=[&](double t){
        const auto wa=at(fa,t),wb=at(fb,t),mid=(wa+wb)*.5;
        const auto worldA=ra*glm::dvec3(wa,0),worldB=ra*glm::dvec3(wb,0),worldMid=ra*glm::dvec3(mid,0);
        Contact c;c.hit=true;c.preciseNormal=ra*glm::dvec3(n,0);c.normal=glm::vec3(c.preciseNormal);
        c.signedSeparation=gap;c.penetration=float(-gap);c.hasLocalAnchors=true;
        c.localAnchorA=glm::dvec3(mid,0);c.localAnchorB=glm::transpose(rb)*(worldMid-delta);
        c.localWitnessA=glm::dvec3(wa,0);c.localWitnessB=glm::transpose(rb)*(worldB-delta);
        c.point=glm::vec3(glm::dvec3(pa.position)+translationA+worldMid);
        c.separationState=gap>0?SeparationState::Separated:gap<0?SeparationState::Penetrating:SeparationState::Touching;
        (void)worldA;result.Add(c);
    };
    if(lo<=hi){emit(lo);if(hi-lo>1e-8)emit(hi);}else emit((lo+hi)*.5);
    return result;
}

bool PlanarOneWayAllows(const Shape& platform,const ContactPose& pp,const Shape& other,const ContactPose& op,double tolerance){
    if(platform.type!=ShapeType::Planar||!platform.planar||!platform.planar->oneWay)return true;
    if(other.type!=ShapeType::Planar||!other.planar)return false;
    const auto pr=ContactRotation(pp.orientation),orr=ContactRotation(op.orientation);
    const auto edge=platform.planar->vertices[1]-platform.planar->vertices[0];
    const auto n=pr*glm::dvec3(glm::normalize(glm::dvec2(-edge.y,edge.x)),0);
    const auto origin=glm::dvec3(pp.position)+pr*(glm::dvec3(platform.planar->vertices[0],0)+glm::dvec3(pp.localCenter));
    double support=INFINITY;
    for(auto p:other.planar->vertices){const auto v=glm::dvec3(op.position)+orr*(glm::dvec3(p,0)+glm::dvec3(op.localCenter));support=std::min(support,glm::dot(v-origin,n));}
    return support-other.planar->radius>=-tolerance;
}

PrimitiveCastHit CastPlanarPrimitive(const Shape& query,const BodyTransform& pose,glm::vec3 direction,double maximum,const PrimitivePose& target){
    if(query.type!=ShapeType::Planar||target.shape.type!=ShapeType::Planar)return {};
    double time=0;
    const ContactPose a{pose.position,pose.rotation,{0,0,0}};
    const ContactPose b{target.parentPosition,target.parentOrientation,target.parentLocalCenter};
    if(!PlanarOneWayAllows(target.shape,b,query,a,0.002))return {};
    for(int iteration=0;iteration<256;++iteration){
        const auto manifold=PlanarContacts(query,a,target.shape,b,INFINITY,glm::dvec3(direction)*time);
        if(!manifold.count)return {};
        const auto& contact=manifold.points[0];
        // Same 10 micrometre convergence tolerance as the existing spatial
        // snapshot casts. This is not simulation skin or CCD relaxation.
        if(contact.signedSeparation<=1e-5){PrimitiveCastHit hit;hit.hit=true;hit.initialOverlap=time==0&&contact.signedSeparation<=0;hit.distance=time;hit.normal=contact.normal;
            hit.point=glm::vec3(glm::dvec3(target.parentPosition)+ContactRotation(target.parentOrientation)*contact.localWitnessB);return hit;}
        const double closing=-glm::dot(contact.preciseNormal,glm::dvec3(direction));
        if(closing<=0)return {};
        const double next=time+contact.signedSeparation/closing;
        if(next>maximum+1e-5)return {};
        time=std::min(next,maximum);
    }
    throw std::runtime_error("2D shape cast did not converge within 256 iterations");
}
