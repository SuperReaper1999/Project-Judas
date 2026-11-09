#include "PhysicsCastGeometry.h"
#include "CollisionGeometry.h"
#include "CollisionAsset.h"
#include "RadialTerrain.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
constexpr double tolerance=1e-5; // metres, query convergence only; no simulation skin
struct Distance {double gap=0;glm::dvec3 normal{0},point{0};};

// Exact segment/AABB closest pair: distance squared is a piecewise quadratic.
// Evaluate interval endpoints and each quadratic minimum. Unlike the player's
// eight alternating projections, this supports long public casts without an
// overestimated separation stepping past a thin collider. Player code is untouched.
Distance SegmentBox(glm::dvec3 a,glm::dvec3 b,double radius,glm::dvec3 h) {
    const auto v=b-a;
    std::vector<double> cuts{0,1};
    for(int k=0;k<3;++k)if(v[k]!=0)for(double face:{-h[k],h[k]}) {
        const double t=(face-a[k])/v[k];if(t>0&&t<1)cuts.push_back(t);
    }
    std::sort(cuts.begin(),cuts.end());
    double best=std::numeric_limits<double>::infinity();glm::dvec3 p{0},q{0};
    auto evaluate=[&](double t){auto x=a+t*v,y=glm::clamp(x,-h,h);double d=glm::dot(x-y,x-y);if(d<best){best=d;p=x;q=y;}};
    for(size_t i=1;i<cuts.size();++i){double lo=cuts[i-1],hi=cuts[i],mid=(lo+hi)*.5,A=0,B=0;
        for(int k=0;k<3;++k){double x=a[k]+mid*v[k];if(x<-h[k]||x>h[k]){double c=a[k]-(x<0?-h[k]:h[k]);A+=v[k]*v[k];B+=v[k]*c;}}
        evaluate(lo);evaluate(hi);if(A>0)evaluate(std::clamp(-B/A,lo,hi));
    }
    if(best>0){double d=std::sqrt(best);return {d-radius,(p-q)/d,q};}
    auto depths=h-glm::abs(p);int k=0;if(depths.y<depths[k])k=1;if(depths.z<depths[k])k=2;
    glm::dvec3 n{0};n[k]=p[k]>=0?1:-1;q=p;q[k]=n[k]*h[k];
    return {-depths[k]-radius,n,q};
}
Distance SegmentSphere(glm::dvec3 a,glm::dvec3 b,double radius,double targetRadius) {
    auto v=b-a;double length2=glm::dot(v,v),t=length2>0?std::clamp(-glm::dot(a,v)/length2,0.,1.):0;
    auto p=a+t*v;double d=glm::length(p);glm::dvec3 n=d>0?p/d:glm::dvec3(1,0,0);
    return {d-radius-targetRadius,n,n*targetRadius};
}
PrimitiveCastHit MakeHit(double t,const Distance& d,bool overlap) {
    return {true,overlap,t,glm::vec3(d.point),glm::vec3(d.normal)};
}
}

PrimitiveCastHit CastAgainstPrimitive(const Shape& cast,const BodyTransform& pose,
    const glm::vec3& direction,double maximum,const PrimitivePose& target,bool initialOnly,
    const glm::vec3& normalAxis,float minimumNormalDot) {
    const auto accepted=[&](const PrimitiveCastHit& hit) {
        return hit.hit && (normalAxis==glm::vec3(0) || glm::dot(hit.normal,normalAxis)>minimumNormalDot);
    };
    const auto targetR=PrimitiveRotation(target);
    const glm::dvec3 targetCenter=PrimitiveCenter(target);
    const auto queryR=ContactRotation(pose.rotation);
    const glm::dvec3 origin=glm::dvec3(pose.position)-targetCenter,dir=glm::normalize(glm::dvec3(direction));
    auto distance=[&](double t) {
        glm::dvec3 center=origin+dir*t;
        const double half=cast.type==ShapeType::Capsule?cast.halfHeight:0;
        glm::dvec3 a=center-queryR[1]*half,b=center+queryR[1]*half;
        Distance d;
        if(cast.type!=ShapeType::Box) {
            if(target.shape.type==ShapeType::Capsule){auto x=SegmentGeometry(targetCenter+a,targetCenter+b,cast.radius,target);d={x.gap,x.normal,x.point-targetCenter};}
            else if(target.shape.type==ShapeType::Sphere)d=SegmentSphere(a,b,cast.radius,target.shape.radius);
            else {d=SegmentBox(glm::transpose(targetR)*a,glm::transpose(targetR)*b,cast.radius,glm::dvec3(target.shape.halfExtents));d.normal=targetR*d.normal;d.point=targetR*d.point;}
        } else if(target.shape.type==ShapeType::Capsule) {
            // Reverse segment/OBB distance, preserving the real capsule witness.
            auto axis=targetR[1]*double(target.shape.halfHeight);
            auto reverse=SegmentBox(glm::transpose(queryR)*(-axis-center),glm::transpose(queryR)*(axis-center),target.shape.radius,glm::dvec3(cast.halfExtents));
            d.gap=reverse.gap;d.normal=-queryR*reverse.normal;
            // The reverse box witness and separation reconstruct its nearest
            // target capsule surface, including initial overlap.
            d.point=center+queryR*reverse.point+d.normal*reverse.gap;
        } else if(target.shape.type==ShapeType::Sphere) {
            // Reverse sphere/box distance and convert its target witness back to
            // the sphere surface. This is the same physical sphere/OBB geometry.
            auto sphereLocal=glm::transpose(queryR)*(-center);
            auto reverse=SegmentBox(sphereLocal,sphereLocal,target.shape.radius,glm::dvec3(cast.halfExtents));
            d.gap=reverse.gap;d.normal=-queryR*reverse.normal;d.point=d.normal*double(target.shape.radius);
        } else {
            // Existing robust SAT/contact geometry supplies a separating plane
            // (a lower bound on distance) and real target-local surface witness.
            ContactPose aPose{glm::vec3(center),pose.rotation,{0,0,0}};
            ContactPose bPose{{0,0,0},glm::normalize(target.parentOrientation*target.childRotation),glm::conjugate(target.childRotation)*target.parentLocalCenter};
            // Use parent-relative centres without world float rounding.
            aPose.position=glm::vec3(glm::dvec3(pose.position)+dir*t-glm::dvec3(target.parentPosition));
            auto manifold=PrimitiveContacts(cast,aPose,target.shape,bPose,std::numeric_limits<float>::max());
            if(!manifold.count)throw std::runtime_error("box cast could not evaluate contact geometry");
            const auto& contact=manifold.points[0];d.gap=contact.signedSeparation;d.normal=contact.preciseNormal;
            d.point=targetR*(contact.localWitnessB-glm::dvec3(target.parentLocalCenter));
        }
        d.point+=targetCenter;return d;
    };
    if(target.shape.asset) {
        auto sample=[&](double t,uint32_t feature=UINT32_MAX){const glm::dvec3 center=glm::dvec3(pose.position)+dir*t;
            if(cast.type==ShapeType::Box){RigidBody body;body.position=glm::vec3(center);body.orientation=pose.rotation;return PolyGeometry(PrimitiveAt(cast,body,0),target,maximum-t+1e-5,feature);}
            double half=cast.type==ShapeType::Capsule?cast.halfHeight:0;return SegmentGeometry(center-queryR[1]*half,center+queryR[1]*half,cast.radius,target,maximum-t+1e-5,false,feature);
        };
        auto result=[&](double t,const GeometryDistance& d,bool overlap){PrimitiveCastHit hit{true,overlap,t,glm::vec3(d.point),glm::vec3(d.normal)};hit.feature=d.feature;return hit;};
        if(target.shape.asset->convex){auto first=sample(0);if(first.valid&&first.gap<=0)return result(0,first,true);if(initialOnly)return {};}
        if(cast.type==ShapeType::Sphere&&cast.radius==0) {
            // A ray originating exactly on a mesh surface is already touching,
            // including zero distance or a direction tangent to that surface.
            if(!target.shape.asset->convex){const auto p=glm::dvec3(pose.position);
                auto first=SegmentGeometry(p,p,0,target,0);
                if(first.valid&&first.gap<=0)return result(0,first,true);
            }
            // Triangle intersections have exact geometric sidedness; hulls are
            // closed, while a one-sided open mesh only accepts front-facing rays.
            auto local=glm::transpose(targetR)*origin,velocity=glm::transpose(targetR)*dir;
            glm::dvec3 end=local+velocity*maximum;std::vector<uint32_t> candidates;uint64_t visited=0;target.shape.asset->Candidates(glm::min(local,end),glm::max(local,end),candidates,&visited);CountCollisionQuery(visited,candidates.size());
            PrimitiveCastHit nearest;double best=maximum;
            for(auto i:candidates){const auto& f=target.shape.asset->faces[i];const auto a=target.shape.asset->vertices[f.vertices[0]],b=target.shape.asset->vertices[f.vertices[1]],c=target.shape.asset->vertices[f.vertices[2]];
                const auto e=b-a,g=c-a,p=glm::cross(velocity,g);double det=glm::dot(e,p);if((!target.shape.asset->convex&&!target.shape.asset->twoSided&&det<=1e-12)||std::abs(det)<=1e-12)continue;
                const auto v=local-a;double u=glm::dot(v,p)/det;if(u< -1e-10||u>1+1e-10)continue;auto q=glm::cross(v,e);double w=glm::dot(velocity,q)/det;if(w< -1e-10||u+w>1+1e-10)continue;double t=glm::dot(g,q)/det;if(t<0||t>best)continue;
                if(nearest.hit&&t==best&&i>nearest.feature)continue;
                PrimitiveCastHit candidate{true,false,t,glm::vec3(targetCenter+targetR*(local+velocity*t)),glm::vec3(targetR*(det>0?f.normal:-f.normal)),i};
                if(accepted(candidate)){best=t;nearest=candidate;}
            }return nearest;
        }
        auto advance=[&](uint32_t feature,double bound){
            if(initialOnly){
                auto d=sample(0,feature);
                return d.valid&&d.gap<=0?result(0,d,true):PrimitiveCastHit{};
            }
            double t=0;
            for(unsigned iteration=0;iteration<256;++iteration){
                auto d=sample(t,feature);if(!d.valid)return PrimitiveCastHit{};
                if(d.gap<=tolerance)return result(t,d,t==0&&d.gap<=0);
                double closing=-glm::dot(dir,d.normal);if(closing<=0)return PrimitiveCastHit{};
                double next=t+d.gap/closing;if(next>bound+tolerance)return PrimitiveCastHit{};
                t=std::min(next,bound);
            }
            throw std::runtime_error("cooked shape cast exceeded bounded 256-iteration policy");
        };
        if(target.shape.asset->convex)return advance(UINT32_MAX,maximum);
        // A concave mesh is a UNION of bounded physical patches. A separating
        // plane of its closest patch is not a separating plane of the union.
        // Traverse the swept volume once and advance each candidate independently.
        const auto inverse=glm::transpose(targetR);
        glm::dvec3 lo(INFINITY),hi(-INFINITY);
        glm::dvec3 half=cast.type==ShapeType::Box?glm::dvec3(cast.halfExtents):glm::dvec3(cast.radius,cast.radius+cast.halfHeight,cast.radius);
        for(double t:{0.,maximum})for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1}){
            auto p=inverse*(origin+dir*t+queryR*(half*glm::dvec3(x,y,z)));
            lo=glm::min(lo,p);hi=glm::max(hi,p);
        }
        std::vector<uint32_t> candidates;uint64_t visited=0;
        target.shape.asset->Candidates(lo-glm::dvec3(tolerance),hi+glm::dvec3(tolerance),candidates,&visited);
        CountCollisionQuery(visited,0);
        PrimitiveCastHit nearest;double bound=maximum;
        for(auto feature:candidates){auto hit=advance(feature,bound);if(!accepted(hit))continue;if(initialOnly)return hit;if(!nearest.hit||hit.distance<nearest.distance||(hit.distance==nearest.distance&&hit.feature<nearest.feature)){nearest=hit;bound=hit.distance;}}
        return nearest;
    }
    if(target.shape.type==ShapeType::Terrain) {
        if(cast.type==ShapeType::Box)throw std::invalid_argument("box casts against radial terrain are unsupported; use sphere/capsule casts");
        // Same authoritative radial surface and nine core samples as the player
        // query. Terrain has approximate distance, no supplied Lipschitz bound:
        // bracket crossings at a declared finite resolution, not fake exact CCD.
        auto sample=[&](double t){Distance best;best.gap=std::numeric_limits<double>::infinity();
            for(int i=0;i<9;++i){const auto local=glm::transpose(targetR)*(origin+dir*t+queryR[1]*double(cast.halfHeight)*(i/4.-1));
                auto s=target.shape.terrain->Sample(glm::vec3(local));double gap=s.signedDistance-cast.radius;
                if(gap<best.gap)best={gap,targetR*glm::dvec3(s.outwardNormal),targetCenter+targetR*glm::dvec3(s.surfacePoint)};}
            return best;};
        auto first=sample(0);if(first.gap<=0)return MakeHit(0,first,true);
        if(initialOnly)return {};
        // Restrict sampling to the target's actual conservative bounding sphere.
        double radius=target.shape.terrain->BoundRadius()+cast.radius+cast.halfHeight;
        double projection=glm::dot(origin,dir),disc=projection*projection-glm::dot(origin,origin)+radius*radius;
        if(disc<0)return {};
        double lo=std::max(0.,-projection-std::sqrt(disc)),end=std::min(maximum,-projection+std::sqrt(disc));
        if(end<lo)return {};
        for(int i=0;i<=512;++i){double t=lo+(end-lo)*i/512.;auto d=sample(t);
            if(d.gap<=0){double lower=i?lo+(end-lo)*(i-1)/512.:0,upper=t;
                for(int j=0;j<32;++j){double mid=(lower+upper)*.5;if(sample(mid).gap<=0)upper=mid;else lower=mid;}
                return MakeHit(upper,sample(upper),false);}}
        return {};
    }
    if(initialOnly){
        const auto first=distance(0);
        return first.gap<=0?MakeHit(0,first,true):PrimitiveCastHit{};
    }
    // A ray is a radius-zero point. Analytic primitives avoid grazing convergence
    // and detect arbitrarily thin boxes regardless of cast length.
    if(cast.type==ShapeType::Sphere&&cast.radius==0) {
        if(target.shape.type==ShapeType::Sphere) {
            auto start=distance(0);if(start.gap<=0)return MakeHit(0,start,true);
            double projection=glm::dot(origin,dir),disc=projection*projection-glm::dot(origin,origin)+double(target.shape.radius)*target.shape.radius;
            if(disc<0)return {};
            double t=-projection-std::sqrt(disc);if(t<0||t>maximum)return {};return MakeHit(t,distance(t),false);
        }
        if(target.shape.type==ShapeType::Capsule){
            auto start=distance(0);if(start.gap<=0)return MakeHit(0,start,true);
            const auto o=glm::transpose(targetR)*origin,v=glm::transpose(targetR)*dir;
            const double r=target.shape.radius,h=target.shape.halfHeight;
            double best=INFINITY;
            auto roots=[&](double A,double B,double C,const auto& accepts){
                if(A<=1e-24)return;
                double disc=B*B-A*C;if(disc<0)return;
                for(double t:{(-B-std::sqrt(disc))/A,(-B+std::sqrt(disc))/A})
                    if(t>=0&&t<=maximum&&t<best&&accepts(o+v*t))best=t;
            };
            roots(v.x*v.x+v.z*v.z,o.x*v.x+o.z*v.z,o.x*o.x+o.z*o.z-r*r,[&](auto p){return p.y>=-h&&p.y<=h;});
            for(double end:{-h,h}){auto a=o-glm::dvec3(0,end,0);roots(glm::dot(v,v),glm::dot(a,v),glm::dot(a,a)-r*r,[&](auto p){return end<0?p.y<=-h:p.y>=h;});}
            return std::isfinite(best)?MakeHit(best,distance(best),false):PrimitiveCastHit{};
        }
        auto o=glm::transpose(targetR)*origin,v=glm::transpose(targetR)*dir,h=glm::dvec3(target.shape.halfExtents);
        auto start=distance(0);if(start.gap<=0)return MakeHit(0,start,true);
        double entry=0,exit=maximum;
        for(int k=0;k<3;++k){if(v[k]==0){if(o[k]<-h[k]||o[k]>h[k])return {};continue;}
            double t0=(-h[k]-o[k])/v[k],t1=(h[k]-o[k])/v[k];if(t0>t1)std::swap(t0,t1);entry=std::max(entry,t0);exit=std::min(exit,t1);if(entry>exit)return {};}
        return MakeHit(entry,distance(entry),false);
    }
    double t=0;
    for(int i=0;i<256;++i){auto d=distance(t);if(d.gap<=tolerance)return MakeHit(t,d,t==0&&d.gap<=0);
        // Fixed orientations: the current separating plane remains valid until
        // translation closes its gap. Never step beyond that plane's crossing.
        double closing=-glm::dot(dir,d.normal);if(closing<=0)return {};
        double next=t+d.gap/closing;if(next>maximum+tolerance)return {};t=std::min(next,maximum);
    }
    throw std::runtime_error("shape cast did not converge within 256 iterations");
}
