#pragma once
#include "Contacts.h"
#include "ContactGeometryInternal.h"
#include <cfenv>

// RESEARCH PROTOTYPE ONLY. Supplies a sufficient bound that preserves a
// legacy interval's resolved sign; it never supplies zero, a response gap,
// witness, clamp branch, tolerance or a collision decision by itself.
// See PROOF.txt for the arithmetic/range boundary and explicit fallback scope.
class SpherePredicateFilter {
public:
    static constexpr int Unresolved=2;
    SpherePredicateFilter(const ContactPose& a,const ContactPose& b,
                          const contact_geometry::Pair<double>& number) {
        if(std::fegetround()!=FE_TONEAREST)return;
        if(!Bounded(a)||!Bounded(b))return;
        const double da=2*number.a.d,db=2*number.b.d;
        if(!(da>0)||!(db>0)||!std::isfinite(da)||!std::isfinite(db))return;
        denominator=da*db;
        boxDenominator=denominator*db;
        positioned=L1(a.position)+L1(b.position)+3*(L1(a.localCenter)+L1(b.localCenter));
        valid=std::isfinite(denominator)&&std::isfinite(boxDenominator)&&std::isfinite(positioned);
    }
    int Sphere(double value,float firstRadius,float secondRadius,float margin=0)const {
        if(!valid||!PositiveInput(firstRadius)||!PositiveInput(secondRadius)||!Margin(margin))return Unresolved;
        const double radius=double(firstRadius)+double(secondRadius)+double(margin);
        const double a=denominator*positioned,b=denominator*radius;
        return Sign(value,2*(a*a+b*b));
    }
    int SphereBox(double value,float radius,const glm::vec3& half,float margin=0)const {
        if(!valid||!PositiveInput(radius)||!Bounded(half)||
           glm::any(glm::lessThanEqual(half,glm::vec3(0)))||!Margin(margin))return Unresolved;
        const double a=boxDenominator*(3*positioned+L1(half));
        const double b=boxDenominator*(double(radius)+double(margin));
        return Sign(value,2*(a*a+b*b));
    }
private:
    static constexpr double Gamma(unsigned n){
        constexpr double u=std::numeric_limits<double>::epsilon()*.5;
        return (n*u)/(1-n*u);
    }
    static int Sign(double value,double magnitude){
        if(!std::isfinite(value)||!std::isfinite(magnitude)||!(magnitude>0))return Unresolved;
        const double error=Gamma(16384)*magnitude;
        if(value>error)return 1;
        if(value< -error)return -1;
        return Unresolved;
    }
    static bool Bounded(double value){return value==0 || (std::isfinite(value)&&std::abs(value)>=0x1p-48&&std::abs(value)<=0x1p48);}
    static bool Bounded(glm::vec3 v){return Bounded(double(v.x))&&Bounded(double(v.y))&&Bounded(double(v.z));}
    static bool Bounded(const ContactPose& p){return Bounded(p.position)&&Bounded(p.localCenter)&&Bounded(double(p.orientation.w))&&Bounded(double(p.orientation.x))&&Bounded(double(p.orientation.y))&&Bounded(double(p.orientation.z));}
    static bool PositiveInput(float value){return value>0&&Bounded(double(value));}
    static bool Margin(float value){return value>=0&&Bounded(double(value));}
    static double L1(glm::vec3 value){return std::abs(double(value.x))+std::abs(double(value.y))+std::abs(double(value.z));}
    double denominator=0,boxDenominator=0,positioned=0;
    bool valid=false;
};
