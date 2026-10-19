#pragma once
// ISOLATED RESEARCH PROTOTYPE. No production/public pose ABI changes.
#include "Contacts.h"
#include "RigidMotion.h"
#include "ContactGeometryInternal.h"
#include <cfenv>
#include <optional>

namespace continuous_box_ccd {
// Guard each operand/result before the established Iv kernels can propagate
// a nonfinite endpoint. With endpoints <=2^500, internal products stay below
// 2^1001 and TwoSum intermediates below 2^503. Division may overflow, but with
// finite inputs and a nonzero denominator cannot create NaN; its result is
// rejected immediately. This is an arithmetic support boundary, not a gap or
// collision tolerance. Underflow retains Iv's existing outward enclosure.
class CheckedInterval : public contact_geometry::Iv {
    void Check() const {
        if(!std::isfinite(lo)||!std::isfinite(hi)||lo>hi||
           std::abs(lo)>0x1p500||std::abs(hi)>0x1p500)
            throw contact_geometry::ArithmeticFailure();
    }
public:
    CheckedInterval() = default;
    CheckedInterval(double value):contact_geometry::Iv(value){Check();}
    CheckedInterval(contact_geometry::Iv value):contact_geometry::Iv(value){Check();}
};
using Iv=CheckedInterval;
inline Iv operator-(Iv a){return -static_cast<contact_geometry::Iv>(a);}
inline Iv operator+(Iv a,Iv b){return static_cast<contact_geometry::Iv>(a)+static_cast<contact_geometry::Iv>(b);}
inline Iv operator-(Iv a,Iv b){return a+-b;}
inline Iv operator*(Iv a,Iv b){return static_cast<contact_geometry::Iv>(a)*static_cast<contact_geometry::Iv>(b);}
inline Iv operator/(Iv a,Iv b){return static_cast<contact_geometry::Iv>(a)/static_cast<contact_geometry::Iv>(b);}
inline Iv Abs(Iv value){return contact_geometry::Abs(static_cast<contact_geometry::Iv>(value));}
inline Iv Sqrt(Iv value){return contact_geometry::Sqrt(static_cast<contact_geometry::Iv>(value));}

// The caller supplies a pure box with zero pivot. Exactly one source is set.
// A dynamic segment is its recorded anchor/generator, never a sampled float pose.
struct MotionView {
    const RigidMotionSegment *segment = nullptr;
    const RigidBody *staticBody = nullptr;
    glm::vec3 halfExtents{0};
};
struct PointResult {
    bool supported = false, strictlySeparated = false;
    glm::dvec3 worldAxis{0}; // A separating plane; A is on its positive side.
    double gapLower = 0;    // Certified Euclidean separation lower bound.
};
struct PrefixResult {
    bool supported = false, separated = false;
    unsigned leaves = 0, nodes = 0, maximumDepth = 0;
};

namespace detail {
using namespace contact_geometry;
using Iv=continuous_box_ccd::Iv;
constexpr unsigned MaximumDepth = 3;
struct Source {
    glm::vec3 position{0}, velocity{0}, omega{0};
    glm::quat orientation{1, 0, 0, 0};
    double begin = 0;
};
inline bool Finite(glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
inline bool Finite(Iv value) {
    return std::isfinite(value.lo) && std::isfinite(value.hi) && value.lo <= value.hi;
}
inline bool Read(const MotionView &view, double start, double finish, Source &out) {
    if (std::fegetround() != FE_TONEAREST || !std::isfinite(start) ||
        !std::isfinite(finish) || finish < start || !Finite(view.halfExtents) ||
        glm::any(glm::lessThanEqual(view.halfExtents, glm::vec3(0))) ||
        (view.segment == nullptr) == (view.staticBody == nullptr))
        return false;
    if (view.segment) {
        const auto &s = *view.segment;
        if (s.prescribed || !std::isfinite(s.begin) || !std::isfinite(s.end) ||
            start < s.begin || finish > s.end || !std::isfinite(s.inverseMass))
            return false;
        out.position = s.position;
        out.orientation = s.orientation;
        out.begin = s.begin;
        if (s.inverseMass > 0) {
            out.velocity = s.linearVelocity;
            out.omega = s.driftAngularVelocity;
        }
    } else {
        out.position = view.staticBody->position;
        out.orientation = view.staticBody->orientation;
    }
    const auto &q = out.orientation;
    if(!std::isfinite(q.w)||!std::isfinite(q.x)||!std::isfinite(q.y)||!std::isfinite(q.z))return false;
    const Iv norm = Iv(double(q.w))*Iv(double(q.w)) + Iv(double(q.x))*Iv(double(q.x)) +
                    Iv(double(q.y))*Iv(double(q.y)) + Iv(double(q.z))*Iv(double(q.z));
    return Finite(out.position) && Finite(out.velocity) && Finite(out.omega) &&
           Finite(norm) && norm.lo > 0;
}

template<class T> std::array<T, 4> Derivative(const Source &source) {
    const T w(double(source.orientation.w)), x(double(source.orientation.x)),
            y(double(source.orientation.y)), z(double(source.orientation.z));
    const T a(double(source.omega.x)), b(double(source.omega.y)), c(double(source.omega.z));
    return {-(a*x+b*y+c*z)*T(.5), (a*w+b*z-c*y)*T(.5),
            (-a*z+b*w+c*x)*T(.5), (a*y-b*x+c*w)*T(.5)};
}
template<class T> struct Frame { M<T> n; T d; };
template<class T> Frame<T> FrameAt(const Source &source, const T &elapsed) {
    const auto dq = Derivative<T>(source);
    const std::array<T, 4> q{T(double(source.orientation.w))+dq[0]*elapsed,
                           T(double(source.orientation.x))+dq[1]*elapsed,
                           T(double(source.orientation.y))+dq[2]*elapsed,
                           T(double(source.orientation.z))+dq[3]*elapsed};
    const auto &w=q[0], &x=q[1], &y=q[2], &z=q[3];
    return {{V<T>{w*w+x*x-y*y-z*z,T(2)*(x*y+w*z),T(2)*(x*z-w*y)},
             V<T>{T(2)*(x*y-w*z),w*w-x*x+y*y-z*z,T(2)*(y*z+w*x)},
             V<T>{T(2)*(x*z+w*y),T(2)*(y*z-w*x),w*w-x*x-y*y+z*z}},
            w*w+x*x+y*y+z*z};
}
// Subtract the exact anchors before adding either local displacement. Large
// common world positions do not enter either projection or polynomial bound.
template<class T> V<T> Relative(const Source &a, const Source &b,
                                const T &elapsedA) {
    V<T> result;
    for (unsigned k=0;k<3;++k)
        result[k] = T(double(a.position[k])) - T(double(b.position[k])) +
                    (T(double(a.velocity[k]))-T(double(b.velocity[k])))*elapsedA +
                    T(double(b.velocity[k]))*(T(b.begin)-T(a.begin));
    return result;
}
inline bool ValidAxis(glm::dvec3 n) {
    return std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z) &&
           std::max({std::abs(n.x),std::abs(n.y),std::abs(n.z)}) > 0;
}
inline Iv PlaneGap(const Frame<Iv> &a, const Frame<Iv> &b, const V<Iv> &relative,
                   glm::vec3 ha, glm::vec3 hb, glm::dvec3 normal) {
    const V<Iv> n{Iv(normal.x),Iv(normal.y),Iv(normal.z)};
    Iv gap = Dot(relative,n);
    for (unsigned k=0;k<3;++k)
        gap = gap - Iv(double(ha[k]))*Abs(Dot(a.n[k],n))/a.d -
                    Iv(double(hb[k]))*Abs(Dot(b.n[k],n))/b.d;
    return gap / Sqrt(Dot(n,n));
}

// Degree <=5 power coefficients on the normalized interval x in [0,1].
// No truncation is permitted: overflowing the declared degree is unsupported.
struct Polynomial {
    std::array<Iv, 6> c{};
    unsigned degree = 0;
    Polynomial() = default;
    Polynomial(double value) { c[0]=Iv(value); }
    explicit Polynomial(Iv value) { c[0]=value; }
    Polynomial(Iv constant, Iv slope):degree(1) { c[0]=constant;c[1]=slope; }
};
inline Polynomial operator-(Polynomial a) {
    for (unsigned k=0;k<=a.degree;++k) a.c[k]=-a.c[k];
    return a;
}
inline Polynomial operator+(const Polynomial &a,const Polynomial &b) {
    Polynomial out;out.degree=std::max(a.degree,b.degree);
    for(unsigned k=0;k<=out.degree;++k)out.c[k]=a.c[k]+b.c[k];
    return out;
}
inline Polynomial operator-(const Polynomial &a,const Polynomial &b) { return a+-b; }
inline Polynomial operator*(const Polynomial &a,const Polynomial &b) {
    Polynomial out;out.degree=a.degree+b.degree;
    if(out.degree>5)throw ArithmeticFailure();
    for(unsigned i=0;i<=a.degree;++i)for(unsigned j=0;j<=b.degree;++j)
        out.c[i+j]=out.c[i+j]+a.c[i]*b.c[j];
    return out;
}
using Bernstein = std::array<Iv,6>;
inline Bernstein Convert(const Polynomial &p) {
    constexpr unsigned choose[6][6]={{1,0,0,0,0,0},{1,1,0,0,0,0},
        {1,2,1,0,0,0},{1,3,3,1,0,0},{1,4,6,4,1,0},{1,5,10,10,5,1}};
    Bernstein out{};
    for(unsigned k=0;k<6;++k)for(unsigned i=0;i<=k;++i)
        out[k]=out[k]+p.c[i]*(Iv(double(choose[k][i]))/Iv(double(choose[5][i])));
    return out;
}
inline bool FixedPlane(const MotionView &a,const MotionView &b,const Source &sa,
                       const Source &sb,double start,double finish,glm::dvec3 axis) {
    if(!ValidAxis(axis))return false;
    const Iv duration=Iv(finish)-Iv(start);
    const Polynomial ta(Iv(start)-Iv(sa.begin),duration),tb(Iv(start)-Iv(sb.begin),duration);
    const auto ra=FrameAt<Polynomial>(sa,ta), rb=FrameAt<Polynomial>(sb,tb);
    const V<Polynomial> n{Polynomial(axis.x),Polynomial(axis.y),Polynomial(axis.z)};
    const auto relative=Relative<Polynomial>(sa,sb,ta);
    const auto common=Convert(Dot(relative,n)*(ra.d*rb.d));
    std::array<Bernstein,6> radii;
    for(unsigned k=0;k<3;++k) {
        radii[k]=Convert(Dot(ra.n[k],n)*rb.d*Polynomial(double(a.halfExtents[k])));
        radii[k+3]=Convert(Dot(rb.n[k],n)*ra.d*Polynomial(double(b.halfExtents[k])));
    }
    for(unsigned k=0;k<6;++k) {
        Iv lower=common[k];
        // This is exactly the coefficientwise minimum over all 8*8 choices
        // of box corners. Every corner polynomial must have positive bounds.
        for(const auto &radius:radii)lower=lower-Abs(radius[k]);
        if(!Finite(lower)||!(lower.lo>0))return false;
    }
    // q=q0+(.5*Omega*q0)*tau has |q|^2=|q0|^2*(1+.25*|Omega|^2*tau^2)>0.
    // Read already certified |q0|^2>0. Clearing these denominators preserves
    // the sign for the whole interval, not just sampled endpoint poses.
    return true;
}
} // namespace detail

inline PointResult Point(const MotionView &a,const MotionView &b,double time) {
    using namespace detail;
    PointResult result;Source sa,sb;
    if(!Read(a,time,time,sa)||!Read(b,time,time,sb))return result;
    result.supported=true;
    try {
        const auto ra=FrameAt<double>(sa,time-sa.begin),rb=FrameAt<double>(sb,time-sb.begin);
        const auto delta=Relative<double>(sa,sb,time-sa.begin);
        Pair<double> pair(ContactPose{},ContactPose{});
        pair.a.n=ra.n;pair.a.d=ra.d;pair.b.n=rb.n;pair.b.d=rb.d;
        pair.denominator=ra.d*rb.d;pair.delta=Mul(delta,pair.denominator);
        glm::dvec3 best(0);double bestGap=0;
        for(int index=0;index<15;++index) {
            const auto raw=Axis(pair,index);
            glm::dvec3 axis(raw[0],raw[1],raw[2]);
            const double length=glm::length(axis);
            if(!(length>0)||!std::isfinite(length))continue;
            const double gap=Sat(pair,a.halfExtents,b.halfExtents,index)/(pair.denominator*length);
            if(gap>bestGap) {
                if(Dot(delta,raw)<0)axis=-axis;
                best=axis/length;bestGap=gap;
            }
        }
        if(!ValidAxis(best))return result;
        const auto ia=FrameAt<Iv>(sa,Iv(time)-Iv(sa.begin));
        const auto ib=FrameAt<Iv>(sb,Iv(time)-Iv(sb.begin));
        const auto relative=Relative<Iv>(sa,sb,Iv(time)-Iv(sa.begin));
        const Iv gap=PlaneGap(ia,ib,relative,a.halfExtents,b.halfExtents,best);
        if(Finite(gap)&&gap.lo>0) {
            result.strictlySeparated=true;result.worldAxis=best;result.gapLower=gap.lo;
        }
    } catch(const ArithmeticFailure &) { result.supported=false; }
    return result;
}

// A conservative alternative when the bounded polynomial prefix proof is
// inconclusive. The caller's point must be Point(a,b,time). Every corner has
// normal approach speed <= |relative velocity| + |omegaA|*radiusA +
// |omegaB|*radiusB. Cayley motion has instantaneous angular speed
// |omega|/(1+.25*|omega|^2*tau^2), hence never exceeds the recorded generator.
// Gap division AND addition to absolute time are outward rounded downward.
// Uncertain arithmetic/nonprogress returns no result, never "no collision".
inline std::optional<double> SafeAdvance(const MotionView &a,const MotionView &b,
                                          double time,double finish,const PointResult &point) {
    using namespace detail;
    Source sa,sb;
    if(!point.supported||!point.strictlySeparated||!(point.gapLower>0)||
       !std::isfinite(point.gapLower)||!Read(a,time,finish,sa)||!Read(b,time,finish,sb))
        return std::nullopt;
    try {
        V<Iv> relative;
        for(unsigned k=0;k<3;++k)relative[k]=Iv(double(sa.velocity[k]))-Iv(double(sb.velocity[k]));
        Iv speed=sa.velocity==sb.velocity?Iv(0):Sqrt(Dot(relative,relative));
        auto angular=[&](const Source &source,glm::vec3 half) {
            if(source.omega==glm::vec3(0))return Iv(0);
            const auto omega=Vector<Iv>(source.omega),extent=Vector<Iv>(half);
            return Sqrt(Dot(omega,omega))*Sqrt(Dot(extent,extent));
        };
        speed=speed+angular(sa,a.halfExtents)+angular(sb,b.halfExtents);
        if(!Finite(speed)||speed.hi<0)return std::nullopt;
        if(speed.hi==0)return finish>time?std::optional<double>(finish):std::nullopt;
        const Iv duration=Iv(point.gapLower)/Iv(speed.hi);
        if(!Finite(duration)||!(duration.lo>0))return std::nullopt;
        const Iv absolute=Iv(time)+Iv(duration.lo);
        if(!Finite(absolute))return std::nullopt;
        const double next=std::min(finish,absolute.lo);
        return next>time?std::optional<double>(next):std::nullopt;
    } catch(const ArithmeticFailure &) { return std::nullopt; }
}

namespace detail {
inline bool Certify(const MotionView &a,const MotionView &b,const Source &sa,const Source &sb,
                    double start,double finish,glm::dvec3 axis,unsigned depth,PrefixResult &result) {
    ++result.nodes;result.maximumDepth=std::max(result.maximumDepth,depth);
    if(!ValidAxis(axis)) {
        const auto point=Point(a,b,start+(finish-start)*.5);
        if(!point.supported){result.supported=false;return false;}
        if(!point.strictlySeparated)return false;
        axis=point.worldAxis;
    }
    if(FixedPlane(a,b,sa,sb,start,finish,axis)) {++result.leaves;return true;}
    if(depth==MaximumDepth)return false;
    const double middle=start+(finish-start)*.5;
    if(!(middle>start&&middle<finish))return false;
    return Certify(a,b,sa,sb,start,middle,glm::dvec3(0),depth+1,result)&&
           Certify(a,b,sa,sb,middle,finish,glm::dvec3(0),depth+1,result);
}
} // namespace detail
inline PrefixResult SeparatedPrefix(const MotionView &a,const MotionView &b,
                                    double start,double finish,glm::dvec3 axis=glm::dvec3(0)) {
    PrefixResult result;detail::Source sa,sb;
    if(!detail::Read(a,start,finish,sa)||!detail::Read(b,start,finish,sb))return result;
    result.supported=true;
    try {result.separated=detail::Certify(a,b,sa,sb,start,finish,axis,0,result);}
    catch(const contact_geometry::ArithmeticFailure &){result.supported=false;result.separated=false;}
    return result;
}
} // namespace continuous_box_ccd
