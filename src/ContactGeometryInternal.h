#pragma once
// FTFT4A internal arithmetic. IEEE binary64 round-to-nearest, no fast-math.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include <stdexcept>

namespace contact_geometry {
    struct ArithmeticFailure : std::runtime_error {
        ArithmeticFailure():std::runtime_error("unresolved contact arithmetic range"){}
    };

    static_assert(sizeof(double)==sizeof(std::uint64_t) && std::numeric_limits<double>::is_iec559,
    "contact interval endpoints require IEEE binary64");
    // Exact adjacent representable values, equivalent to nextafter toward +/-inf.
    // Implemented directly to avoid a libm call at every interval endpoint.
    inline double Down(double x) {
        if (std::isnan(x) || x==-std::numeric_limits<double>::infinity()) return x;
        if (x==0) return -std::numeric_limits<double>::denorm_min();
        std::uint64_t bits;
        std::memcpy(&bits,&x,sizeof bits);
        if (x>0)--bits;
        else ++bits;
        std::memcpy(&x,&bits,sizeof x);
        return x;
    }

    inline double Up(double x) {
        if (std::isnan(x) || x==std::numeric_limits<double>::infinity()) return x;
        if (x==0) return std::numeric_limits<double>::denorm_min();
        std::uint64_t bits;
        std::memcpy(&bits,&x,sizeof bits);
        if (x>0)++bits;
        else --bits;
        std::memcpy(&x,&bits,sizeof x);
        return x;
    }

    struct Iv {
        double lo=0,hi=0;
        Iv()=default;
        Iv(double x):lo(x),hi(x){}
        Iv(double l,double h):lo(l),hi(h){}
    };

    inline Iv operator-(Iv a) {
        return {-a.hi,-a.lo};
    }

    inline Iv operator+(Iv a,Iv b) {
        if (a.lo==0 && a.hi==0) return b;
        if (b.lo==0 && b.hi==0) return a;
        if (a.lo==a.hi && b.lo==b.hi) {
            const double s=a.lo+b.lo, bv=s-a.lo;
            const double e=(a.lo-(s-bv))+(b.lo-bv);
            if (std::isfinite(s)) return {e<0?Down(s):s,e>0?Up(s):s};
        }

        return {Down(a.lo+b.lo),Up(a.hi+b.hi)};
    }

    inline Iv operator-(Iv a,Iv b) {
        return a+-b;
    }

    // Exact exponent test for normal binary64 operands. Subnormals retain
    // ilogb; no arithmetic bound, predicate or floating-point order changes.
    inline bool ProductErrorIsNormalRange(double a,double b){
        std::uint64_t aa,bb;std::memcpy(&aa,&a,8);std::memcpy(&bb,&b,8);
        const unsigned ea=unsigned((aa>>52)&2047),eb=unsigned((bb>>52)&2047);
        if(ea&&ea<2047&&eb&&eb<2047)return ea+eb>=1078; // (ea-1023)+(eb-1023)>=-968
        return std::ilogb(std::abs(a))+std::ilogb(std::abs(b))>=-968;
    }
    inline Iv operator*(Iv a,Iv b) {
        if ((a.lo==0&&a.hi==0)||(b.lo==0&&b.hi==0)) return Iv(0);
        if (a.lo==1&&a.hi==1)return b;
        if (b.lo==1&&b.hi==1)return a;
        if (a.lo==-1&&a.hi==-1)return -b;
        if (b.lo==-1&&b.hi==-1)return -a;
        if (a.lo==a.hi && b.lo==b.hi) {
            const double p=a.lo*b.lo,e=std::fma(a.lo,b.lo,-p);
            if (std::isfinite(p) && std::abs(p)>=std::numeric_limits<double>::min() &&
            ProductErrorIsNormalRange(a.lo,b.lo))
            return {e<0?Down(p):p,e>0?Up(p):p};
        }

        // A scalar times an interval has only two distinct endpoint products.
        // Keep the point/point error-free product above and the exact same
        // outward endpoint rounding as the four-product reference below.
        if(a.lo==a.hi&&std::isfinite(a.lo)&&std::isfinite(b.lo)&&std::isfinite(b.hi)){
            const double x=a.lo*b.lo,y=a.lo*b.hi;
            return {Down(std::min(x,y)),Up(std::max(x,y))};
        }
        if(b.lo==b.hi&&std::isfinite(b.lo)&&std::isfinite(a.lo)&&std::isfinite(a.hi)){
            const double x=a.lo*b.lo,y=a.hi*b.lo;
            return {Down(std::min(x,y)),Up(std::max(x,y))};
        }
        const std::array<double,4> v{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};
        return {Down(*std::min_element(v.begin(),v.end())),Up(*std::max_element(v.begin(),v.end()))};
    }

    inline Iv operator/(Iv a,Iv b) {
        if (b.lo<=0 && b.hi>=0) throw ArithmeticFailure();
        if (b.lo==1&&b.hi==1)return a;
        if(b.lo==b.hi&&std::isfinite(b.lo)&&std::isfinite(a.lo)&&std::isfinite(a.hi)){
            const double x=a.lo/b.lo,y=a.hi/b.lo;
            return {Down(std::min(x,y)),Up(std::max(x,y))};
        }
        if(a.lo==a.hi&&std::isfinite(a.lo)&&std::isfinite(b.lo)&&std::isfinite(b.hi)){
            const double x=a.lo/b.lo,y=a.lo/b.hi;
            return {Down(std::min(x,y)),Up(std::max(x,y))};
        }
        const std::array<double,4> v{a.lo/b.lo,a.lo/b.hi,a.hi/b.lo,a.hi/b.hi};
        return {Down(*std::min_element(v.begin(),v.end())),Up(*std::max_element(v.begin(),v.end()))};
    }

    inline Iv Abs(Iv a) {
        if (a.lo>=0)return a;
        if (a.hi<=0)return -a;
        return {0,std::max(-a.lo,a.hi)};
    }

    inline Iv Positive(Iv a) {
        return {std::max(a.lo,0.0),std::max(a.hi,0.0)};
    }

    inline Iv Sqrt(Iv a) {
        if (a.hi<0)throw ArithmeticFailure();
        return {a.lo<=0?0:Down(std::sqrt(a.lo)),Up(std::sqrt(std::max(a.hi,0.0)))};
    }

    inline double Abs(double a){
        return std::abs(a);
    }

    inline double Positive(double a){
        return std::max(a,0.0);
    }

    // Nonoverlapping expansion; no fixed precision silently declares zero.
    struct Exact {
        std::vector<double> e;
        Exact()=default;
        Exact(double a) {
            if (!std::isfinite(a))throw ArithmeticFailure();
            if (a!=0)e.push_back(a);
        }

        void Grow(double b) {
            std::vector<double> out;
            out.reserve(e.size()+1);
            double q=b;
            for (double a:e) {
                const double s=q+a,bv=s-q,err=(q-(s-bv))+(a-bv);
                if (!std::isfinite(s))throw ArithmeticFailure();
                if (err!=0)out.push_back(err);
                q=s;
            }

            if (q!=0)out.push_back(q);
            e=std::move(out);
        }

        int Sign()const {
            return e.empty()?0:(e.back()>0?1:-1);
        }

        long double Value()const {
            long double x=0;
            for (double a:e)x+=static_cast<long double>(a);
            return x;
        }
    };

    inline Exact operator-(Exact a){
        for (double& x:a.e)x=-x;
        return a;
    }

    inline Exact operator+(Exact a,const Exact& b){
        for (double x:b.e)a.Grow(x);
        return a;
    }

    inline Exact operator-(Exact a,const Exact& b){
        for (double x:b.e)a.Grow(-x);
        return a;
    }

    inline Exact operator*(const Exact& a,const Exact& b){
        Exact r;
        for (double x:a.e)for (double y:b.e){
            const double p=x*y;
            if (!std::isfinite(p)||std::abs(p)<std::numeric_limits<double>::min())throw ArithmeticFailure();
            const double err=std::fma(x,y,-p);
            // In the supported normal-product range FMA's residual is exact unless
            // it underflows. Reject that range conservatively, including zero residual.
            if (std::ilogb(std::abs(x))+std::ilogb(std::abs(y)) < -968)throw ArithmeticFailure();
            if (err!=0)r.Grow(err);
            r.Grow(p);
        }

        return r;
    }

    inline Exact Abs(Exact a){
        return a.Sign()<0?-a:a;
    }

    inline Exact Positive(Exact a){
        return a.Sign()<0?Exact(0):a;
    }

    template<class T> using V=std::array<T,3>;
    template<class T> using M=std::array<V<T>,3>;
    // columns
    template<class T> V<T> Add(const V<T>&a,const V<T>&b){
        return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};
    }

    template<class T> V<T> Sub(const V<T>&a,const V<T>&b){
        return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};
    }

    template<class T> V<T> Mul(const V<T>&a,const T&s){
        return {a[0]*s,a[1]*s,a[2]*s};
    }

    template<class T> T Dot(const V<T>&a,const V<T>&b){
        return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
    }

    template<class T> V<T> Cross(const V<T>&a,const V<T>&b){
        return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
    }

    template<class T> V<T> Transform(const M<T>&m,const V<T>&v){
        return Add(Add(Mul(m[0],v[0]),Mul(m[1],v[1])),Mul(m[2],v[2]));
    }

    template<class T> V<T> Vector(const glm::vec3&v){
        return {T(double(v.x)),T(double(v.y)),T(double(v.z))};
    }

    template<class T> struct Rotation {
        M<T> n;
        T d;
        explicit Rotation(const glm::quat&q){
            const T w(double(q.w)),x(double(q.x)),y(double(q.y)),z(double(q.z)),two(2);
            d=w*w+x*x+y*y+z*z;
            n={
                V<T>{w*w+x*x-y*y-z*z,two*(x*y+w*z),two*(x*z-w*y)},
                V<T>{two*(x*y-w*z),w*w-x*x+y*y-z*z,two*(y*z+w*x)},
                V<T>{two*(x*z+w*y),two*(y*z-w*x),w*w-x*x-y*y+z*z}
            };
        }
    };

    template<class T> struct Pair {
        Rotation<T> a,b;
        V<T> delta;
        T denominator;
        Pair(const ContactPose& pa, const ContactPose& pb)
            : Pair(pa, pb, Rotation<T>(pa.orientation), Rotation<T>(pb.orientation)) {}

        Pair(const ContactPose& pa, const ContactPose& pb,
             const Rotation<T>& preparedA, const Rotation<T>& preparedB)
            : a(preparedA), b(preparedB) {
            denominator=a.d*b.d;
            delta=Add(Mul(Sub(Vector<T>(pa.position),Vector<T>(pb.position)),denominator),
            Sub(Mul(Transform(a.n,Vector<T>(pa.localCenter)),b.d),Mul(Transform(b.n,Vector<T>(pb.localCenter)),a.d)));
        }
    };

    template<class T> V<T> Axis(const Pair<T>&p,int index){
        if (index<3)return p.a.n[index];
        if (index<6)return p.b.n[index-3];
        return Cross(p.a.n[(index-6)/3],p.b.n[(index-6)%3]);
    }

    template<class T> T Sat(const Pair<T>&p,const glm::vec3&ha,const glm::vec3&hb,int axis){
        const auto t=Axis(p,axis);
        T s=Abs(Dot(p.delta,t));
        for (int k=0;k<3;++k)s=s-T(double(ha[k]))*Abs(Dot(p.a.n[k],t))*p.b.d-T(double(hb[k]))*Abs(Dot(p.b.n[k],t))*p.a.d;
        return s;
    }

    // FTFT4A-P: identical homogeneous polynomial, factored using N^T N=d^2 I.
    // Original Sat remains the unchanged exact fallback and magnitude path.
    template<class T> struct SatWorkspace {
        const Pair<T>& p;
        mutable std::array<std::array<T,3>,3> ab;
        mutable unsigned preparedRows=0;
        T da2,db2;
        explicit SatWorkspace(const Pair<T>& pair):p(pair),da2(p.a.d*p.a.d),db2(p.b.d*p.b.d){}
        void PrepareRow(int i)const{
            const unsigned bit=1u<<i;
            if(preparedRows&bit)return;
            for(int j=0;j<3;++j)ab[i][j]=Dot(p.a.n[i],p.b.n[j]);
            preparedRows|=bit;
        }
        T Evaluate(const glm::vec3&ha,const glm::vec3&hb,int axis,const V<T>&t)const{
            // A-face axes need only their own cross-orientation row. Most
            // separated pairs stop here; retain the other six dot products
            // until a later axis actually needs them. Every expression and
            // its rounding order is unchanged from eager preparation.
            if(axis<3)PrepareRow(axis);
            else if(preparedRows!=7u)for(int i=0;i<3;++i)PrepareRow(i);
            T s=Abs(Dot(p.delta,t));
            if(axis<3){
                for(int k=0;k<3;++k){
                    const T own = k==axis ? da2 : T(0);
                    s=s-T(double(ha[k]))*own*p.b.d-T(double(hb[k]))*Abs(ab[axis][k])*p.a.d;
                }
            }else if(axis<6){
                const int j=axis-3;
                for(int k=0;k<3;++k){
                    const T own = k==j ? db2 : T(0);
                    s=s-T(double(ha[k]))*Abs(ab[k][j])*p.b.d-T(double(hb[k]))*own*p.a.d;
                }
            }else{
                const int i=(axis-6)/3,j=(axis-6)%3;
                for(int k=0;k<3;++k){
                    const T aa=k==i ? T(0) : p.a.d*Abs(ab[3-k-i][j]);
                    const T bb=k==j ? T(0) : p.b.d*Abs(ab[i][3-k-j]);
                    s=s-T(double(ha[k]))*aa*p.b.d-T(double(hb[k]))*bb*p.a.d;
                }
            }
            return s;
        }
    };

    template<class T> T SpherePredicate(const Pair<T>&p,const T& r){
        return Dot(p.delta,p.delta)-r*r*p.denominator*p.denominator;
    }

    template<class T> T SphereBoxPredicate(const Pair<T>&p,const glm::vec3&half,const T& r){
        const T h=p.denominator*p.b.d;
        T s(0);
        for (int k=0;k<3;++k){
            const T a=Positive(Abs(Dot(p.b.n[k],p.delta))-T(double(half[k]))*h);
            s=s+a*a;
        }

        return s-r*r*h*h;
    }
}

// namespace contact_geometry
