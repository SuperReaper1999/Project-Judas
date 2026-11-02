#pragma once

// Independent TEST-ONLY volume measurement. This integrates a DECLARED union of
// geometric cells, not a continuum water surface, pressure field or wetness fit.
// Missing history/model error is NOT bounded by the numerical interval below.
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace FluidCoverageObservation {
struct Box {
    glm::dvec3 center{0};
    glm::dvec3 half{1};
    glm::dquat rotation{1, 0, 0, 0};
};
struct Options { unsigned maxDepth=6; std::size_t maxNodes=100000; };
struct Result {
    double lowerFraction=0, upperFraction=1;
    double unresolvedFractionUpper=1;
    std::size_t nodes=0, cellTests=0, insideLeaves=0, outsideLeaves=0, unresolvedLeaves=0;
    bool budgetExhausted=false, depthExhausted=false;
};
namespace Detail {
// Each endpoint encloses real arithmetic on the supplied binary64 inputs.
// Basic IEEE operations/sqrt must have <=1 ulp error; finite overflow is rejected.
// No fused or fast-math reassociation is compatible with this diagnostic.
struct Interval { double lo, hi; };
inline double Down(double x) {
    if(!std::isfinite(x))throw std::invalid_argument("Coverage arithmetic overflow");
    return std::nextafter(x,-std::numeric_limits<double>::infinity());
}
inline double Up(double x) {
    if(!std::isfinite(x))throw std::invalid_argument("Coverage arithmetic overflow");
    return std::nextafter(x,std::numeric_limits<double>::infinity());
}
inline bool Point(Interval a,double x){return a.lo==x&&a.hi==x;}
inline Interval Neg(Interval a){return {-a.hi,-a.lo};}
inline Interval Add(Interval a,Interval b) {
    if(Point(a,0))return b;
    if(Point(b,0))return a;
    return {Down(a.lo+b.lo),Up(a.hi+b.hi)};
}
inline Interval Sub(Interval a,Interval b){return Add(a,Neg(b));}
inline Interval Mul(Interval a,Interval b) {
    if(Point(a,0)||Point(b,0))return {0,0};
    if(Point(a,1))return b;
    if(Point(b,1))return a;
    if(Point(a,-1))return Neg(b);
    if(Point(b,-1))return Neg(a);
    const std::array<double,4> p{{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi}};
    return {Down(*std::min_element(p.begin(),p.end())),Up(*std::max_element(p.begin(),p.end()))};
}
inline Interval DividePositive(Interval a,Interval b) {
    if(b.lo<=0)throw std::invalid_argument("Coverage denominator is unresolved");
    if(Point(b,1))return a;
    return Mul(a,{Down(1/b.hi),Up(1/b.lo)});
}
inline Interval Sqrt(Interval a) {
    if(a.lo<0)throw std::invalid_argument("Coverage quaternion normalization unresolved");
    if(Point(a,0)||Point(a,1))return a;
    return {std::max(0.,Down(std::sqrt(a.lo))),Up(std::sqrt(a.hi))};
}
using Matrix=std::array<std::array<Interval,3>,3>;
using Vector=std::array<Interval,3>;
inline void Validate(const Box& box) {
    for(int k=0;k<3;++k)
        if(!std::isfinite(box.center[k])||!std::isfinite(box.half[k])||box.half[k]<=0)
            throw std::invalid_argument("Coverage requires finite centers and positive half extents");
    for(int k=0;k<4;++k)if(!std::isfinite(box.rotation[k]))
        throw std::invalid_argument("Coverage quaternion must be finite");
}
inline Matrix Rotation(const glm::dquat& rotation) {
    std::array<Interval,4> q{{{rotation.w,rotation.w},{rotation.x,rotation.x},
                              {rotation.y,rotation.y},{rotation.z,rotation.z}}};
    Interval norm2{0,0};for(const auto v:q)norm2=Add(norm2,Mul(v,v));
    // Squaring an interval can straddle zero after outward rounding. Actual
    // norm2 is nonnegative; zero/underflow quaternions remain rejected.
    norm2.lo=std::max(0.,norm2.lo);
    const auto norm=Sqrt(norm2);
    for(auto& v:q)v=DividePositive(v,norm);
    const auto w=q[0],x=q[1],y=q[2],z=q[3];
    const Interval one{1,1},two{2,2};
    Matrix r;
    r[0][0]=Sub(one,Mul(two,Add(Mul(y,y),Mul(z,z))));
    r[0][1]=Mul(two,Sub(Mul(x,y),Mul(z,w)));
    r[0][2]=Mul(two,Add(Mul(x,z),Mul(y,w)));
    r[1][0]=Mul(two,Add(Mul(x,y),Mul(z,w)));
    r[1][1]=Sub(one,Mul(two,Add(Mul(x,x),Mul(z,z))));
    r[1][2]=Mul(two,Sub(Mul(y,z),Mul(x,w)));
    r[2][0]=Mul(two,Sub(Mul(x,z),Mul(y,w)));
    r[2][1]=Mul(two,Add(Mul(y,z),Mul(x,w)));
    r[2][2]=Sub(one,Mul(two,Add(Mul(x,x),Mul(y,y))));
    return r;
}
struct Cell { Matrix bodyToCell; Vector offset; glm::dvec3 half; Vector bodyBounds; };
inline Cell TransformCell(const Box& body,const Matrix& bodyRotation,const Box& box) {
    Validate(box);const auto rotation=Rotation(box.rotation);
    Cell cell;cell.half=box.half;
    for(int i=0;i<3;++i) {
        cell.offset[i]={0,0};
        Interval center{0,0};
        for(int k=0;k<3;++k) {
            const auto difference=Sub({body.center[k],body.center[k]},{box.center[k],box.center[k]});
            cell.offset[i]=Add(cell.offset[i],Mul(rotation[k][i],difference));
            center=Add(center,Mul(bodyRotation[k][i],Neg(difference)));
        }
        Interval extent{0,0};
        for(int j=0;j<3;++j) {
            Interval a{0,0},inverse{0,0};
            for(int k=0;k<3;++k) {
                a=Add(a,Mul(rotation[k][i],bodyRotation[k][j]));
                inverse=Add(inverse,Mul(bodyRotation[k][i],rotation[k][j]));
            }
            cell.bodyToCell[i][j]=a;
            extent=Add(extent,Mul(inverse,{-box.half[j],box.half[j]}));
        }
        cell.bodyBounds[i]=Add(center,extent);
    }
    return cell;
}
struct Node { glm::dvec3 lo,hi;unsigned depth;std::vector<std::size_t> candidates; };
enum class Relation { Outside, Inside, Unknown };
inline Relation Classify(const Node& node,const Cell& cell) {
    bool inside=true;
    for(int i=0;i<3;++i) {
        // Touching boundary planes have zero 3-D volume; tangent cells need
        // not keep a positive-volume node unresolved forever.
        if(node.lo[i]>=cell.bodyBounds[i].hi||node.hi[i]<=cell.bodyBounds[i].lo)
            return Relation::Outside;
        auto range=cell.offset[i];
        for(int j=0;j<3;++j)
            range=Add(range,Mul(cell.bodyToCell[i][j],{node.lo[j],node.hi[j]}));
        if(range.lo>=cell.half[i]||range.hi<=-cell.half[i])return Relation::Outside;
        inside&=range.lo>=-cell.half[i]&&range.hi<=cell.half[i];
    }
    return inside?Relation::Inside:Relation::Unknown;
}
inline Interval Volume(const Node& node) {
    Interval value{1,1};
    for(int k=0;k<3;++k)value=Mul(value,Sub({node.hi[k],node.hi[k]},{node.lo[k],node.lo[k]}));
    if(value.lo<=0)throw std::invalid_argument("Coverage volume underflow/unresolved");
    return value;
}
} // namespace Detail

inline Result Measure(const Box& body,const std::vector<Box>& wetCells,Options options={}) {
    using namespace Detail;
    static_assert(std::numeric_limits<double>::is_iec559,"Coverage requires IEEE binary64");
#if defined(__FAST_MATH__) || defined(_M_FP_FAST)
    throw std::invalid_argument("Coverage intervals cannot execute with fast-math");
#endif
    if(options.maxNodes<1||options.maxDepth>20)
        throw std::invalid_argument("Coverage needs positive node budget and depth <=20");
    Validate(body);const auto bodyRotation=Rotation(body.rotation);
    std::vector<Cell> cells;cells.reserve(wetCells.size());
    for(const auto& wet:wetCells)cells.push_back(TransformCell(body,bodyRotation,wet));
    Node root{-body.half,body.half,0,{}};
    for(std::size_t i=0;i<cells.size();++i)root.candidates.push_back(i);
    const auto bodyVolume=Volume(root);
    std::deque<Node> pending;pending.push_back(std::move(root));
    Interval covered{0,0},unresolved{0,0};std::size_t created=1;
    Result result;
    while(!pending.empty()) {
        Node node=std::move(pending.front());pending.pop_front();++result.nodes;
        std::vector<std::size_t> candidates;bool inside=false;
        for(const auto index:node.candidates) {
            ++result.cellTests;const auto relation=Classify(node,cells[index]);
            if(relation==Relation::Inside){inside=true;break;}
            if(relation==Relation::Unknown)candidates.push_back(index);
        }
        if(inside){covered=Add(covered,Volume(node));++result.insideLeaves;continue;}
        if(candidates.empty()){++result.outsideLeaves;continue;}
        if(node.depth==options.maxDepth||options.maxNodes-created<8) {
            unresolved=Add(unresolved,Volume(node));++result.unresolvedLeaves;
            result.depthExhausted|=node.depth==options.maxDepth;
            result.budgetExhausted|=node.depth<options.maxDepth;
            continue;
        }
        const auto middle=(node.lo+node.hi)*.5;
        for(int k=0;k<3;++k)if(middle[k]<=node.lo[k]||middle[k]>=node.hi[k])
            throw std::invalid_argument("Coverage subdivision cannot progress");
        for(unsigned bits=0;bits<8;++bits) {
            Node child;child.depth=node.depth+1;child.candidates=candidates;
            for(int k=0;k<3;++k){child.lo[k]=(bits&(1u<<k))?middle[k]:node.lo[k];child.hi[k]=(bits&(1u<<k))?node.hi[k]:middle[k];}
            pending.push_back(std::move(child));
        }
        created+=8;
    }
    const auto fraction=DividePositive(covered,bodyVolume);
    const auto maximum=DividePositive(Add(covered,unresolved),bodyVolume);
    result.lowerFraction=std::clamp(fraction.lo,0.,1.);
    result.upperFraction=std::clamp(maximum.hi,0.,1.);
    result.unresolvedFractionUpper=std::clamp(DividePositive(unresolved,bodyVolume).hi,0.,1.);
    return result;
}
} // namespace FluidCoverageObservation
