#include "Contacts.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include "ContactPreparedGeometry.h"

namespace {
    constexpr float kEpsilon = 1.0e-6f;
    // retained by legacy capsule approximation only
    using namespace contact_geometry;
    thread_local ContactGeometryDiagnostics diagnostics;
    bool Valid(const ContactPose& p) {
        for (int k=0;k<3;++k)if (!std::isfinite(p.position[k])||!std::isfinite(p.localCenter[k]))return false;
        const double w=p.orientation.w,x=p.orientation.x,y=p.orientation.y,z=p.orientation.z;
        return std::isfinite(w)&&std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&(w*w+x*x+y*y+z*z)>0;
    }

    template<class F> int Sign(Iv value,F exact) {
        ++diagnostics.predicates;
        if (std::isfinite(value.lo)&&std::isfinite(value.hi)) {
            if (value.lo>0){
                ++diagnostics.intervalResolved;
                return 1;
            }

            if (value.hi<0){
                ++diagnostics.intervalResolved;
                return -1;
            }

            if (value.lo==0&&value.hi==0){
                ++diagnostics.intervalResolved;
                return 0;
            }
        }

        ++diagnostics.exactFallbacks;
        return exact().Sign();
    }

    struct Context {
        ContactPose pa,pb;
        Pair<Iv> interval;
        Pair<double> number;
        std::unique_ptr<Pair<Exact>> exact;
        glm::dmat3 ra,rb;
        glm::dvec3 a,b;
        Context(const ContactPose& x, const ContactPose& y,
                const ContactPreparedOrientation& preparedA,
                const ContactPreparedOrientation& preparedB)
            : pa(x), pb(y), interval(x,y,preparedA.interval,preparedB.interval),
              number(x,y,preparedA.number,preparedB.number),
              ra(preparedA.rotation), rb(preparedB.rotation),
              a(ra*glm::dvec3(x.localCenter)),
              b(glm::dvec3(y.position)-glm::dvec3(x.position)+rb*glm::dvec3(y.localCenter)) {}
        const Pair<Exact>& E(){
            if (!exact)exact=std::make_unique<Pair<Exact>>(pa,pb);
            return *exact;
        }
    };

    SeparationState State(double gap){
        return gap>0?SeparationState::Separated:gap<0?SeparationState::Penetrating:SeparationState::Touching;
    }

    Contact MakeContact(const Context& c,const glm::dvec3& normal,double gap,const glm::dvec3& witnessA,const glm::dvec3& witnessB) {
        Contact result;
        result.hit=true;
        result.preciseNormal=normal;
        result.normal=glm::vec3(normal);
        result.signedSeparation=gap;
        result.penetration=static_cast<float>(-gap);
        result.separationState=State(gap);
        const glm::dvec3 midpoint=(witnessA+witnessB)*0.5;
        const glm::dvec3 parentDelta=glm::dvec3(c.pb.position)-glm::dvec3(c.pa.position);
        result.localAnchorA=glm::transpose(c.ra)*midpoint;
        result.localAnchorB=glm::transpose(c.rb)*(midpoint-parentDelta);
        result.localWitnessA=glm::transpose(c.ra)*witnessA;
        result.localWitnessB=glm::transpose(c.rb)*(witnessB-parentDelta);
        result.hasLocalAnchors=true;
        result.point=glm::vec3(glm::dvec3(c.pa.position)+midpoint);
        return result;
    }

    bool GapAgreesWithSign(double gap, int sign) {
        return std::isfinite(gap) && (sign>0 ? gap>0 : sign<0 ? gap<0 : gap==0);
    }

    // Rationalization avoids subtracting nearly equal square roots. Exact fallback
    // values are used if the sign filter needed the homogeneous polynomial.
    double SphereGap(Context& c,double distance,double radius,int sign) {
        if (sign==0)return 0;
        long double numerator;
        long double denominator;
        if (c.exact) {
            const auto& e=c.E();
            numerator=SpherePredicate(e,Exact(radius)).Value();
            const long double k=e.denominator.Value();
            denominator=k*k;
        }
        else {
            numerator=SpherePredicate(c.number,radius);
            const long double k=c.number.denominator;
            denominator=k*k;
        }

        double gap=static_cast<double>(numerator/(denominator*(static_cast<long double>(distance)+radius)));
        if (!GapAgreesWithSign(gap,sign)) {
            ++diagnostics.numericGapFallbacks;
            const auto& e=c.E();
            const long double k=e.denominator.Value();
            gap=static_cast<double>(SpherePredicate(e,Exact(radius)).Value()/
                (k*k*(static_cast<long double>(distance)+radius)));
            if (!GapAgreesWithSign(gap,sign)) throw ArithmeticFailure();
        }
        return gap;
    }

    template<bool ExistenceOnly> using PairResult=std::conditional_t<ExistenceOnly,bool,Contact>;

    template<bool ExistenceOnly=false>
    PairResult<ExistenceOnly> SpherePair(Context& c,float ra,float rb,float margin) {
        const Iv r=Iv(double(ra))+Iv(double(rb));
        const Iv pred=SpherePredicate(c.interval,r);
        const int sign=Sign(pred,[&]{
            return SpherePredicate(c.E(),Exact(double(ra))+Exact(double(rb)));
        });
        if (sign>0) {
            const int beyond=Sign(SpherePredicate(c.interval,r+Iv(double(margin))),[&]{
                return SpherePredicate(c.E(),Exact(double(ra))+Exact(double(rb))+Exact(double(margin)));
            });
            if (beyond>0)return {};
        }

        const glm::dvec3 delta=c.a-c.b;
        const double distance=glm::length(delta),radius=double(ra)+double(rb);
        // symmetric coincident convention
        // Existence queries still execute the response path's representable-
        // gap validation even though they do not need the resulting witness.
        [[maybe_unused]] const double gap=SphereGap(c,distance,radius,sign);
        if constexpr(ExistenceOnly)return true;
        else{
            const glm::dvec3 normal=distance>0?delta/distance:glm::dvec3(0,1,0);
            return MakeContact(c,normal,gap,c.a-normal*double(ra),c.b+normal*double(rb));
        }
    }

    template<bool ExistenceOnly=false>
    PairResult<ExistenceOnly> SphereBoxPair(Context& c,float radius,const glm::vec3& half,float margin) {
        const Iv pred=SphereBoxPredicate(c.interval,half,Iv(double(radius)));
        const int sign=Sign(pred,[&]{
            return SphereBoxPredicate(c.E(),half,Exact(double(radius)));
        });
        if (sign>0){
            const int beyond=Sign(SphereBoxPredicate(c.interval,half,Iv(double(radius))+Iv(double(margin))),[&]{
                return SphereBoxPredicate(c.E(),half,Exact(double(radius))+Exact(double(margin)));
            });
            if (beyond>0)return {};
        }

        const glm::dvec3 local=glm::transpose(c.rb)*(c.a-c.b),h(half);
        const glm::dvec3 nearest=glm::clamp(local,-h,h),outside=local-nearest;
        const double distance=glm::length(outside);
        [[maybe_unused]] glm::dvec3 normal,witnessB;
        double gap;
        if (distance>0){
            if constexpr(!ExistenceOnly){
                normal=c.rb*(outside/distance);
                witnessB=c.b+c.rb*nearest;
            }
            if (sign==0)gap=0;
            else if (c.exact){
                const auto&e=c.E();
                const long double k=e.denominator.Value()*e.b.d.Value();
                gap=static_cast<double>(SphereBoxPredicate(e,half,Exact(double(radius))).Value()/(k*k*(static_cast<long double>(distance)+radius)));
            }
            else {
                const long double k=static_cast<long double>(c.number.denominator)*c.number.b.d;
                gap=static_cast<double>(SphereBoxPredicate(c.number,half,double(radius))/(k*k*(static_cast<long double>(distance)+radius)));
            }

            if (!GapAgreesWithSign(gap,sign)) {
                ++diagnostics.numericGapFallbacks;
                const auto& e=c.E();
                const long double k=e.denominator.Value()*e.b.d.Value();
                gap=static_cast<double>(SphereBoxPredicate(e,half,Exact(double(radius))).Value()/
                    (k*k*(static_cast<long double>(distance)+radius)));
                if (!GapAgreesWithSign(gap,sign)) throw ArithmeticFailure();
            }
        }
        else{
            // Center inside/on box: nearest exit face, deterministic ties.
            const glm::dvec3 remaining=h-glm::abs(local);
            int axis=0;
            if (remaining.y<remaining[axis])axis=1;
            if (remaining.z<remaining[axis])axis=2;
            if constexpr(!ExistenceOnly){
                glm::dvec3 ln(0);
                ln[axis]=local[axis]>=0?1:-1;
                normal=c.rb*ln;
                glm::dvec3 point=local;
                point[axis]=ln[axis]*h[axis];
                witnessB=c.b+c.rb*point;
            }
            gap=-(double(radius)+remaining[axis]);
        }

        if constexpr(ExistenceOnly)return true;
        else return MakeContact(c,normal,gap,c.a-normal*double(radius),witnessB);
    }

    struct SatResult {
        bool hit=false;
        int axis=-1;
        double gap=-std::numeric_limits<double>::infinity();
        glm::dvec3 normal{0};
    };

    SatResult BoxSat(Context& c,const glm::vec3& ha,const glm::vec3& hb,float margin) {
        SatResult result;
        double bestFace=-std::numeric_limits<double>::infinity(),bestEdge=bestFace;
        int face=-1,edge=-1;
        glm::dvec3 faceNormal(0),edgeNormal(0);
        const SatWorkspace<Iv> workspace(c.interval);
        for (int axis=0;axis<15;++axis){
            const auto ai=Axis(c.interval,axis);
            const Iv lengthSquared=Dot(ai,ai);
            if (Sign(lengthSquared,[&]{
                auto a=Axis(c.E(),axis);return Dot(a,a);
            })==0)continue;
            const Iv s=workspace.Evaluate(ha,hb,axis,ai);
            bool exactGap=false;
            const int sign=Sign(s,[&]{
                exactGap=true;return Sat(c.E(),ha,hb,axis);
            });
            if (sign>0){
                if (margin==0)return {};
                const Iv m{double(margin)};
                const Iv compare=s*s-m*m*c.interval.denominator*c.interval.denominator*lengthSquared;
                const int beyond=Sign(compare,[&]{
                    const auto&e=c.E();auto t=Axis(e,axis);auto es=Sat(e,ha,hb,axis);Exact em{double(margin)};return es*es-em*em*e.denominator*e.denominator*Dot(t,t);
                });
                if (beyond>0)return {};
            }

            const auto ax=Axis(c.number,axis);
            glm::dvec3 normal(ax[0],ax[1],ax[2]);
            double length=glm::length(normal);
            if (!(length>0)||!std::isfinite(length))throw ArithmeticFailure();
            normal/=length;
            if (glm::dot(normal,c.a-c.b)<0)normal=-normal;
            double gap;
            if (sign==0)gap=0;
            else if (exactGap)gap=static_cast<double>(Sat(c.E(),ha,hb,axis).Value()/(c.E().denominator.Value()*static_cast<long double>(length)));
            else gap=Sat(c.number,ha,hb,axis)/(c.number.denominator*length);
            if (!GapAgreesWithSign(gap,sign)) {
                ++diagnostics.numericGapFallbacks;
                const auto& e=c.E();
                gap=static_cast<double>(Sat(e,ha,hb,axis).Value()/
                    (e.denominator.Value()*static_cast<long double>(length)));
                if (!GapAgreesWithSign(gap,sign)) throw ArithmeticFailure();
            }
            if (axis<6 && gap>bestFace){
                bestFace=gap;
                face=axis;
                faceNormal=normal;
            }

            if (axis>=6 && gap>bestEdge){
                bestEdge=gap;
                edge=axis;
                edgeNormal=normal;
            }
        }

        if (face<0)throw ArithmeticFailure();
        result.hit=true;
        // A stable face preference applies ONLY to penetration. Every separating
        // or exactly touching axis retains its physical sign and is eligible.
        const bool useEdge=edge>=0 && (bestEdge>=0?bestEdge>bestFace:bestEdge>0.95*bestFace+0.001);
        result.axis=useEdge?edge:face;
        result.gap=useEdge?bestEdge:bestFace;
        result.normal=useEdge?edgeNormal:faceNormal;
        return result;
    }

    std::pair<glm::dvec3,glm::dvec3> SupportEdge(const glm::dvec3&center,const glm::dmat3&r,const glm::vec3&h,int axis,const glm::dvec3& direction){
        glm::dvec3 c=center;
        for (int k=0;k<3;++k)if (k!=axis)c+=r[k]*(double(h[k])*(glm::dot(r[k],direction)>=0?1:-1));
        const glm::dvec3 e=r[axis]*double(h[axis]);
        return {c-e,c+e};
    }

    std::pair<glm::dvec3,glm::dvec3> SegmentPair(glm::dvec3 p,glm::dvec3 p1,glm::dvec3 q,glm::dvec3 q1){
        const glm::dvec3 u=p1-p,v=q1-q,w=p-q;
        const double a=glm::dot(u,u),b=glm::dot(u,v),c=glm::dot(v,v),d=glm::dot(u,w),e=glm::dot(v,w);
        double best=std::numeric_limits<double>::infinity();
        std::pair<glm::dvec3,glm::dvec3> out;
        auto offer=[&](double s,double t){
            auto x=p+s*u,y=q+t*v;
            const double distance=glm::dot(x-y,x-y);
            if (distance<best){
                best=distance;
                out={x,y};
            }
        };

        offer(0,c>0?std::clamp(e/c,0.,1.):0);
        offer(1,c>0?std::clamp((e+b)/c,0.,1.):0);
        offer(a>0?std::clamp(-d/a,0.,1.):0,0);
        offer(a>0?std::clamp((b-d)/a,0.,1.):0,1);
        const long double determinant=static_cast<long double>(a)*c-static_cast<long double>(b)*b;
        if (determinant>0){
            double s=static_cast<double>((static_cast<long double>(b)*e-static_cast<long double>(c)*d)/determinant);
            double t=static_cast<double>((static_cast<long double>(a)*e-static_cast<long double>(b)*d)/determinant);
            if (s>=0&&s<=1&&t>=0&&t<=1)offer(s,t);
        }

        return out;
    }

    ContactManifold BoxPair(Context& c,const glm::vec3& ha,const glm::vec3& hb,float margin){
        ContactManifold manifold;
        const auto sat=BoxSat(c,ha,hb,margin);
        if (!sat.hit)return manifold;
        if (sat.axis>=6){
            const int ia=(sat.axis-6)/3,ib=(sat.axis-6)%3;
            const auto ea=SupportEdge(c.a,c.ra,ha,ia,-sat.normal),eb=SupportEdge(c.b,c.rb,hb,ib,sat.normal);
            const auto points=SegmentPair(ea.first,ea.second,eb.first,eb.second);
            manifold.Add(MakeContact(c,sat.normal,sat.gap,points.first,points.second));
            return manifold;
        }

        const bool referenceA=sat.axis<3;
        const int r=sat.axis%3,u=(r+1)%3,v=(r+2)%3;
        const glm::dmat3& rr=referenceA?c.ra:c.rb;
        const glm::dmat3& ri=referenceA?c.rb:c.ra;
        const glm::dvec3 rc=referenceA?c.a:c.b,ic=referenceA?c.b:c.a;
        const glm::vec3 rh=referenceA?ha:hb,ih=referenceA?hb:ha;
        const glm::dvec3 outward=referenceA?-sat.normal:sat.normal;
        // Entire polygon is expressed in the reference box's local coordinates.
        const glm::dmat3 relative=glm::transpose(rr)*ri;
        const glm::dvec3 center=glm::transpose(rr)*(ic-rc);
        const double faceSign=glm::dot(rr[r],outward)>=0?1:-1;
        int incident=0;
        for (int k=1;k<3;++k)if (std::abs(relative[k][r])>std::abs(relative[incident][r]))incident=k;
        const double incSign=relative[incident][r]*faceSign>0?-1:1;
        const glm::dvec3 fc=center+relative[incident]*(incSign*double(ih[incident]));
        const glm::dvec3 eu=relative[(incident+1)%3]*double(ih[(incident+1)%3]),ev=relative[(incident+2)%3]*double(ih[(incident+2)%3]);
        std::array<glm::dvec3,16> polygon{},next{};
        int count=4;
        polygon[0]=fc+eu+ev;
        polygon[1]=fc-eu+ev;
        polygon[2]=fc-eu-ev;
        polygon[3]=fc+eu-ev;
        auto clip=[&](int k,double sign,double limit){
            int n=0;
            for (int i=0;i<count;++i){
                const auto&a=polygon[i];
                const auto&b=polygon[(i+1)%count];
                const double da=sign*a[k]-limit,db=sign*b[k]-limit;
                if (da<=0)next[n++]=a;
                if ((da<0&&db>0)||(da>0&&db<0)){
                    auto x=a+(b-a)*(da/(da-db));
                    x[k]=sign*limit;
                    next[n++]=x;
                }
            }

            count=n;
            polygon=next;
        };

        clip(u,1,rh[u]);
        clip(u,-1,rh[u]);
        clip(v,1,rh[v]);
        clip(v,-1,rh[v]);
        std::array<Contact,16> candidates{};
        int n=0;
        for (int i=0;i<count;++i){
            const auto& p=polygon[i];
            double gap=faceSign*p[r]-double(rh[r]);
            // A resolved separating SAT axis cannot produce a touching or
            // penetrating feature. If construction arithmetic contradicts
            // that truth, expose uncertainty instead of inventing zero gap.
            if (sat.gap>0 && gap<=0) throw ArithmeticFailure();
            if (gap>double(margin))continue;
            glm::dvec3 ref=p;
            ref[r]=faceSign*double(rh[r]);
            const glm::dvec3 wp=rc+rr*p,wr=rc+rr*ref;
            candidates[n++]=MakeContact(c,sat.normal,gap,referenceA?wr:wp,referenceA?wp:wr);
        }

        if (n==0){
            // Closest feature witnesses for a separated proximity candidate. This
            // is geometry, not a zero-gap fallback; preserve the selected SAT gap.
            double best=std::numeric_limits<double>::infinity();
            glm::dvec3 wa,wb;
            auto offer=[&](glm::dvec3 a,glm::dvec3 b){
                double d=glm::dot(a-b,a-b);
                if (d<best){
                    best=d;
                    wa=a;
                    wb=b;
                }
            };

            for (int signs=0;signs<8;++signs){
                glm::dvec3 la,lb;
                for (int k=0;k<3;++k){
                    la[k]=((signs>>k)&1)?ha[k]:-ha[k];
                    lb[k]=((signs>>k)&1)?hb[k]:-hb[k];
                }

                auto a=c.a+c.ra*la,b=c.b+c.rb*lb;
                offer(a,c.b+c.rb*glm::clamp(glm::transpose(c.rb)*(a-c.b),-glm::dvec3(hb),glm::dvec3(hb)));
                offer(c.a+c.ra*glm::clamp(glm::transpose(c.ra)*(b-c.a),-glm::dvec3(ha),glm::dvec3(ha)),b);
            }

            manifold.Add(MakeContact(c,sat.normal,sat.gap,wa,wb));
            return manifold;
        }

        if (n<=4){
            for (int i=0;i<n;++i)manifold.Add(candidates[i]);
            return manifold;
        }

        int first=0;
        for (int i=1;i<n;++i)if (candidates[i].signedSeparation<candidates[first].signedSeparation)first=i;
        const auto p0=candidates[first].localAnchorA;
        int second=first==0?1:0;
        for (int i=0;i<n;++i)if (i!=first&&glm::distance(candidates[i].localAnchorA,p0)>glm::distance(candidates[second].localAnchorA,p0))second=i;
        const auto diagonal=candidates[second].localAnchorA-p0;
        const glm::dvec3 ln=glm::transpose(c.ra)*sat.normal;
        int third=-1,fourth=-1;
        double pos=0,neg=0;
        for (int i=0;i<n;++i)if (i!=first&&i!=second){
            double area=glm::dot(glm::cross(diagonal,candidates[i].localAnchorA-p0),ln);
            if (area>pos){
                pos=area;
                third=i;
            }

            if (area<neg){
                neg=area;
                fourth=i;
            }
        }

        manifold.Add(candidates[first]);
        manifold.Add(candidates[second]);
        if (third>=0)manifold.Add(candidates[third]);
        if (fourth>=0)manifold.Add(candidates[fourth]);
        return manifold;
    }
}

ContactGeometryDiagnostics GetContactGeometryDiagnostics(){
    return diagnostics;
}

void ResetContactGeometryDiagnostics(){
    diagnostics={};
}

glm::dmat3 ContactRotation(const glm::quat& q){
    Rotation<double> r(q);
    if (!(r.d>0)||!std::isfinite(r.d))throw std::invalid_argument("contact orientation must be finite and nonzero");
    return glm::dmat3(glm::dvec3(r.n[0][0],r.n[0][1],r.n[0][2])/r.d,glm::dvec3(r.n[1][0],r.n[1][1],r.n[1][2])/r.d,glm::dvec3(r.n[2][0],r.n[2][1],r.n[2][2])/r.d);
}

ContactPreparedOrientation::ContactPreparedOrientation(const glm::quat& orientation)
    : source(orientation), interval(orientation), number(orientation) {
    if (!(number.d>0) || !std::isfinite(number.d))
        throw std::invalid_argument("contact orientation must be finite and nonzero");
    for (int j=0;j<3;++j) {
        rotation[j]=glm::dvec3(number.n[j][0],number.n[j][1],number.n[j][2])/number.d;
        for (int k=0;k<3;++k) normalizedInterval[j][k]=interval.n[j][k]/interval.d;
    }
}

bool ContactPreparedOrientation::Matches(const glm::quat& orientation) const {
    const float a[]={source.w,source.x,source.y,source.z};
    const float b[]={orientation.w,orientation.x,orientation.y,orientation.z};
    return std::memcmp(a,b,sizeof a)==0;
}

ContactManifold PrimitiveContacts(const Shape& a,const ContactPose& pa,const Shape& b,const ContactPose& pb,float margin,
                                  const ContactPreparedOrientation* preparedA,
                                  const ContactPreparedOrientation* preparedB){
    ContactManifold result;
    if (!Valid(pa)||!Valid(pb)){
        ++diagnostics.invalidInputs;
        ++diagnostics.unresolved;
        result.uncertain=true;
        return result;
    }

    try {
        std::optional<ContactPreparedOrientation> localA,localB;
        const auto resolve=[&](const glm::quat& q,const ContactPreparedOrientation* prepared,
                               std::optional<ContactPreparedOrientation>& local)
                               -> const ContactPreparedOrientation& {
            if (prepared && prepared->Matches(q)) return *prepared;
            if (prepared) ++diagnostics.preparedOrientationMisses;
            local.emplace(q);
            return *local;
        };
        const auto& orientationA=resolve(pa.orientation,preparedA,localA);
        const auto& orientationB=resolve(pb.orientation,preparedB,localB);
        if (a.type==ShapeType::Box&&b.type==ShapeType::Sphere){
            Context reverse(pb,pa,orientationB,orientationA);
            Contact contact=SphereBoxPair(reverse,b.radius,a.halfExtents,margin);
            contact.normal=-contact.normal;
            contact.preciseNormal=-contact.preciseNormal;
            std::swap(contact.localAnchorA,contact.localAnchorB);
            std::swap(contact.localWitnessA,contact.localWitnessB);
            result.Add(contact);
        }
        else {
            Context c(pa,pb,orientationA,orientationB);
            if (a.type==ShapeType::Sphere&&b.type==ShapeType::Sphere)result.Add(SpherePair(c,a.radius,b.radius,margin));
            else if (a.type==ShapeType::Sphere&&b.type==ShapeType::Box)result.Add(SphereBoxPair(c,a.radius,b.halfExtents,margin));
            else if (a.type==ShapeType::Box&&b.type==ShapeType::Box)result=BoxPair(c,a.halfExtents,b.halfExtents,margin);
        }
    }
    catch(const ArithmeticFailure&){
        ++diagnostics.unresolved;
        result={};
        result.uncertain=true;
    }

    return result;
}

bool PrimitiveHasContacts(const Shape& a,const ContactPose& pa,const Shape& b,const ContactPose& pb,float margin,
                          const ContactPreparedOrientation* preparedA,
                          const ContactPreparedOrientation* preparedB){
    const auto simple=[](ShapeType type){return type==ShapeType::Sphere||type==ShapeType::Box;};
    if(!simple(a.type)||!simple(b.type)||margin!=0)
        return PrimitiveContacts(a,pa,b,pb,margin,preparedA,preparedB).count>0;
    if(!Valid(pa)||!Valid(pb)){
        ++diagnostics.invalidInputs;
        ++diagnostics.unresolved;
        return false;
    }
    try{
        std::optional<ContactPreparedOrientation> localA,localB;
        const auto resolve=[&](const glm::quat& q,const ContactPreparedOrientation* prepared,
                               std::optional<ContactPreparedOrientation>& local)
                               -> const ContactPreparedOrientation& {
            if(prepared&&prepared->Matches(q))return *prepared;
            if(prepared)++diagnostics.preparedOrientationMisses;
            local.emplace(q);
            return *local;
        };
        const auto& orientationA=resolve(pa.orientation,preparedA,localA);
        const auto& orientationB=resolve(pb.orientation,preparedB,localB);
        if(a.type==ShapeType::Box&&b.type==ShapeType::Sphere){
            Context reverse(pb,pa,orientationB,orientationA);
            return SphereBoxPair<true>(reverse,b.radius,a.halfExtents,margin);
        }
        Context c(pa,pb,orientationA,orientationB);
        if(a.type==ShapeType::Sphere&&b.type==ShapeType::Sphere)
            return SpherePair<true>(c,a.radius,b.radius,margin);
        if(a.type==ShapeType::Sphere&&b.type==ShapeType::Box)
            return SphereBoxPair<true>(c,a.radius,b.halfExtents,margin);
        // Reuse every exact-sign/numeric-gap validation of the response path.
        // With zero margin its accepted SAT gap cannot be positive. BoxPair's
        // only subsequent ArithmeticFailure guard requires a positive SAT gap;
        // every remaining branch emits at least one row. Thus clipping and
        // witness construction cannot change this accepted-contact truth.
        // Positive-margin queries must keep the full construction guard.
        return BoxSat(c,a.halfExtents,b.halfExtents,margin).hit;
    }catch(const ArithmeticFailure&){
        ++diagnostics.unresolved;
        return false;
    }
}

Contact SphereVsSphere(const glm::vec3& a, float ra, const glm::vec3& b, float rb, float margin) {
    const auto m = PrimitiveContacts(Shape::Sphere(ra), {a, glm::quat(1,0,0,0), glm::vec3(0)},
    Shape::Sphere(rb), {b, glm::quat(1,0,0,0), glm::vec3(0)}, margin);
    Contact contact = m.count ? m.points[0] : Contact{};
    contact.anchorAInWorldFrame = true;
    contact.anchorBInWorldFrame = true;
    return contact;
}

Contact SphereVsBox(const glm::vec3& a, float radius, const glm::vec3& b,
const glm::quat& q, const glm::vec3& h, float margin) {
    const auto m = PrimitiveContacts(Shape::Sphere(radius), {a, glm::quat(1,0,0,0), glm::vec3(0)},
    Shape::Box(h), {b, q, glm::vec3(0)}, margin);
    Contact contact = m.count ? m.points[0] : Contact{};
    contact.anchorAInWorldFrame = true;
    return contact;
}

ContactManifold BoxVsBoxManifold(const glm::vec3&a,const glm::quat&qa,const glm::vec3&ha,const glm::vec3&b,const glm::quat&qb,const glm::vec3&hb,float margin){
    return PrimitiveContacts(Shape::Box(ha),{a,qa,glm::vec3(0)},Shape::Box(hb),{b,qb,glm::vec3(0)},margin);
}

Contact BoxVsBox(const glm::vec3&a,const glm::quat&qa,const glm::vec3&ha,const glm::vec3&b,const glm::quat&qb,const glm::vec3&hb,float margin){
    auto m=BoxVsBoxManifold(a,qa,ha,b,qb,hb,margin);
    return m.count?m.points[0]:Contact{};
}

glm::vec3 ClosestPointOnOBB(const glm::vec3& point,const glm::vec3& center,const glm::quat&orientation,const glm::vec3&half){
    const auto r=ContactRotation(orientation);
    const auto local=glm::clamp(glm::transpose(r)*(glm::dvec3(point)-glm::dvec3(center)),-glm::dvec3(half),glm::dvec3(half));
    return glm::vec3(glm::dvec3(center)+r*local);
}

glm::vec3 ClosestPointOnSegment(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 ab = b - a;
    const float lengthSquared = glm::dot(ab, ab);
    if (lengthSquared < kEpsilon) return a;
    const float t = glm::clamp(glm::dot(point - a, ab) / lengthSquared, 0.0f, 1.0f);
    return a + ab * t;
}

void ClosestPointsSegmentToOBB(const glm::vec3& segA, const glm::vec3& segB,
const glm::vec3& boxCenter, const glm::quat& boxOrientation,
const glm::vec3& halfExtents, glm::vec3& outSegmentPoint,
glm::vec3& outBoxPoint) {
    // Alternating projection: start at the segment's midpoint, repeatedly
    // find the closest box point to the current segment point and the
    // closest segment point to that box point. Converges quickly for two
    // convex shapes (each step can only reduce or hold the distance
    // between the two points) -- a handful of iterations is more than
    // enough at this engine's scale.
    glm::vec3 segmentPoint = (segA + segB) * 0.5f;
    glm::vec3 boxPoint = boxCenter;
    for (int i = 0; i < 8; ++i) {
        boxPoint = ClosestPointOnOBB(segmentPoint, boxCenter, boxOrientation, halfExtents);
        segmentPoint = ClosestPointOnSegment(boxPoint, segA, segB);
    }

    outSegmentPoint = segmentPoint;
    outBoxPoint = boxPoint;
}

CapsuleDistance CapsuleDistanceToSphere(const glm::vec3& segA, const glm::vec3& segB,
float capsuleRadius, const glm::vec3& sphereCenter,
float sphereRadius) {
    CapsuleDistance result;
    const glm::vec3 closestOnSegment = ClosestPointOnSegment(sphereCenter, segA, segB);
    const glm::vec3 delta = closestOnSegment - sphereCenter;
    const float centerDistance = glm::length(delta);
    result.distance = centerDistance - capsuleRadius - sphereRadius;
    if (centerDistance > kEpsilon) {
        result.normal = delta / centerDistance;
    }
    else {
        result.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    result.otherPoint = sphereCenter + result.normal * sphereRadius;
    return result;
}

CapsuleDistance CapsuleDistanceToBox(const glm::vec3& segA, const glm::vec3& segB,
float capsuleRadius, const glm::vec3& boxCenter,
const glm::quat& boxOrientation,
const glm::vec3& boxHalfExtents) {
    CapsuleDistance result;
    glm::vec3 segmentPoint, boxPoint;
    ClosestPointsSegmentToOBB(segA, segB, boxCenter, boxOrientation, boxHalfExtents, segmentPoint,
    boxPoint);
    const glm::vec3 delta = segmentPoint - boxPoint;
    float distance = glm::length(delta);
    if (distance > kEpsilon) {
        result.normal = delta / distance;
        result.distance = distance - capsuleRadius;
        result.otherPoint = boxPoint;
        return result;
    }

    // The segment's closest point lies exactly on (or the segment
    // penetrates) the box's surface -- fall back to the deepest-local-axis
    // resolution, same technique as SphereVsBox's degenerate case.
    const glm::vec3 localPoint = glm::conjugate(boxOrientation) * (segmentPoint - boxCenter);
    const glm::vec3 axisPenetration = boxHalfExtents - glm::abs(localPoint);
    int minAxis = 0;
    if (axisPenetration.y < axisPenetration[minAxis]) minAxis = 1;
    if (axisPenetration.z < axisPenetration[minAxis]) minAxis = 2;
    glm::vec3 localNormal(0.0f);
    localNormal[minAxis] = localPoint[minAxis] >= 0.0f ? 1.0f : -1.0f;
    result.normal = boxOrientation * localNormal;
    result.distance = -(axisPenetration[minAxis] + capsuleRadius);
    result.otherPoint = boxPoint;
    return result;
}
