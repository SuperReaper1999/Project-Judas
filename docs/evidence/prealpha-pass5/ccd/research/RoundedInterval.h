#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>
// RESEARCH ONLY. The bound is for nearest-rounded operation outputs, not real
// arithmetic. A production proof would also need operation-order/contraction
// guarantees on every supported compiler. Do not substitute this for Iv.
namespace rounded_episode_probe {
struct RI {double lo=0,hi=0;RI()=default;RI(double x):lo(x),hi(x){}RI(double a,double b):lo(a),hi(b){}};
inline RI operator-(RI a){return {-a.hi,-a.lo};}
inline RI operator+(RI a,RI b){return {a.lo+b.lo,a.hi+b.hi};}
inline RI operator-(RI a,RI b){return a+-b;}
inline RI operator*(RI a,RI b){const std::array<double,4>v{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};return {*std::min_element(v.begin(),v.end()),*std::max_element(v.begin(),v.end())};}
inline RI operator/(RI a,RI b){if(b.lo<=0&&b.hi>=0)throw contact_geometry::ArithmeticFailure();const std::array<double,4>v{a.lo/b.lo,a.lo/b.hi,a.hi/b.lo,a.hi/b.hi};return {*std::min_element(v.begin(),v.end()),*std::max_element(v.begin(),v.end())};}
inline RI NearestSqrt(RI a){if(a.hi<0)throw contact_geometry::ArithmeticFailure();return {std::sqrt(std::max(0.,a.lo)),std::sqrt(std::max(0.,a.hi))};}
inline std::optional<contact_geometry::M<RI>> RoundedRotation(const RigidMotionSegment&segment,double start,double finish){
 if(segment.prescribed||!std::isfinite(start)||!std::isfinite(finish)||start>finish||start<segment.begin||finish>segment.end)return std::nullopt;
 const glm::quat omega(0.f,segment.driftAngularVelocity.x,segment.driftAngularVelocity.y,segment.driftAngularVelocity.z);
 const auto raw=[&](double time){return segment.orientation+(omega*segment.orientation)*(.5f*float(time-segment.begin));};
 const auto first=raw(start),last=raw(finish);
 const std::array<float,4>f{first.w,first.x,first.y,first.z},l{last.w,last.x,last.y,last.z},anchor{segment.orientation.w,segment.orientation.x,segment.orientation.y,segment.orientation.z};
 std::array<float,4>low{},high{},minimum{},maximum{};bool unchanged=true;
 for(unsigned i=0;i<4;++i){low[i]=std::min(f[i],l[i]);high[i]=std::max(f[i],l[i]);minimum[i]=low[i]<=0&&high[i]>=0?0.f:std::min(std::abs(low[i]),std::abs(high[i]));maximum[i]=std::max(std::abs(low[i]),std::abs(high[i]));unchanged=unchanged&&low[i]<=anchor[i]&&high[i]>=anchor[i];}
 const float minimumLength=glm::length(glm::quat(minimum[0],minimum[1],minimum[2],minimum[3])),maximumLength=glm::length(glm::quat(maximum[0],maximum[1],maximum[2],maximum[3]));
 if(!(minimumLength>0)||!std::isfinite(maximumLength))return std::nullopt;
 const float inverseMinimum=1.f/maximumLength,inverseMaximum=1.f/minimumLength;
 std::array<RI,4>q;for(unsigned i=0;i<4;++i){const std::array<float,4>products{low[i]*inverseMinimum,low[i]*inverseMaximum,high[i]*inverseMinimum,high[i]*inverseMaximum};q[i]={double(*std::min_element(products.begin(),products.end())),double(*std::max_element(products.begin(),products.end()))};if(unchanged){q[i].lo=std::min(q[i].lo,double(anchor[i]));q[i].hi=std::max(q[i].hi,double(anchor[i]));}}
 const auto w=q[0],x=q[1],y=q[2],z=q[3],two=RI(2);
 const RI d=w*w+x*x+y*y+z*z;if(!(d.lo>0))return std::nullopt;
 const contact_geometry::M<RI>n={contact_geometry::V<RI>{w*w+x*x-y*y-z*z,two*(x*y+w*z),two*(x*z-w*y)},contact_geometry::V<RI>{two*(x*y-w*z),w*w-x*x+y*y-z*z,two*(y*z+w*x)},contact_geometry::V<RI>{two*(x*z+w*y),two*(y*z-w*x),w*w-x*x-y*y+z*z}};
 contact_geometry::M<RI>r;for(unsigned j=0;j<3;++j)for(unsigned k=0;k<3;++k)r[j][k]=n[j][k]/d;return r;
}
} // namespace rounded_episode_probe
