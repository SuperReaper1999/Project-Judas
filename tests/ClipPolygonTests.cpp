// The same internal kernel invoked by native CollisionGeometry contacts.
// Ordinary header inclusion; no implementation file or engine linker needed.
#include "ClipPolygon.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <cstdio>

namespace {
using V=glm::dvec3;
constexpr double eps=JudasCollisionDetail::kClipEpsilon;
int checks=0,failures=0;
void Check(bool ok,const char* name){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",name);}
void CheckSegment(const std::vector<V>& input,V normal,double plane,const char* label){
    std::vector<V> output;JudasCollisionDetail::ClipPolygonInto(input,normal,plane,output);
    V lo(INFINITY),hi(-INFINITY);for(auto p:input){lo=glm::min(lo,p);hi=glm::max(hi,p);}
    bool finite=true,inSegment=true;double minimumY=INFINITY;
    for(auto p:output){for(unsigned k=0;k<3;++k){finite&=std::isfinite(p[k]);inSegment&=p[k]>=lo[k]&&p[k]<=hi[k];}minimumY=std::min(minimumY,p.y);}
    std::printf("CLIP_CASE %s points=%zu min_y=%.17g input_min_y=%.17g\n",label,output.size(),minimumY,lo.y);
    Check(!output.empty()&&finite,"native clipped output remains nonempty and finite");
    Check(inSegment,"native clipping retains convexity instead of extrapolating beyond incident segment");
    // The front depth of a real triangle face is minus this y projection.
    // Every source point is at y>=0; plane intersection may not invent depth.
    Check(minimumY>=lo.y,"native clipped face-witness depth cannot exceed source support-plane depth");
}
}
int main(){
    // Both endpoints are geometrically outside x<=0, but the existing eps
    // classifier retains the first and rejects the second. An intersection
    // at plane0 has t=-.5/1.5 and extrapolates to y=-.5. This is the real
    // kernel's branch, with exactly its own eps constant.
    CheckSegment({V(.5*eps,0,0),V(1.5*eps,1,0)},V(1,0,0),0,"eps strip .5->1.5");
    CheckSegment({V(1.5*eps,1,0),V(.5*eps,0,0)},V(1,0,0),0,"eps strip reversed");
    CheckSegment({V(.999*eps,0,0),V(1.001*eps,1,0)},V(1,0,0),0,"near-eps strip has large false witness depth");

    // Genuine plane crossings already have t in[0,1]. The correction must
    // retain their EXACT prior floating expression/result, not move them to
    // the eps boundary or alter which source points the classifier retains.
    for(const auto pair:{std::pair<double,double>{-2,3},{3,-2},{-eps,3*eps},{3*eps,-eps}}){
        const V a(pair.first,0,0),b(pair.second,1,0);const double t=pair.first/(pair.first-pair.second);
        const V expected=a+(b-a)*t;std::vector<V> output;JudasCollisionDetail::ClipPolygonInto({a,b},V(1,0,0),0,output);
        bool found=false;for(auto p:output)found|=p==expected;
        Check(found,"genuine plane crossing preserves exact original intersection point");
    }
    std::vector<V> inside={V(-1,0,0),V(.5*eps,1,0)},out;
    JudasCollisionDetail::ClipPolygonInto(inside,V(1,0,0),0,out);Check(out==inside,"existing eps-inclusive retained vertices remain exact");
    JudasCollisionDetail::ClipPolygonInto({V(2*eps,0,0),V(3*eps,1,0)},V(1,0,0),0,out);Check(out.empty(),"wholly outside segment remains absent");
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
