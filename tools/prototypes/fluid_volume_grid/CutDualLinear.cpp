#include "CutDualLinear.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Finite(Real x){return std::isfinite(x.hi)&&std::isfinite(x.lo);}
bool Fail(std::string& e,const char* t){e=t;return false;}
double Dot(const std::vector<double>& a,const std::vector<double>& b){
    std::array<double,4> sums{};for(std::size_t i=0;i<a.size();++i)sums[i%4]+=a[i]*b[i];return (sums[0]+sums[1])+(sums[2]+sums[3]);
}
double Maximum(const std::vector<double>& a){double m=0.;for(double x:a)m=std::max(m,std::abs(x));return m;}
}
bool CutDualLinear::Build(const std::vector<Real>& diagonal,const std::vector<CutDirectedEdge>& edges,std::string& error){
    if(diagonal.empty())return Fail(error,"empty dual momentum matrix");
    CutDualLinear next;next.diagonal_=diagonal;const auto n=diagonal.size();
    std::vector<std::vector<std::pair<std::size_t,Real>>> rows(n);
    for(auto d:diagonal)if(!(d.Value()>0.&&Finite(d)))return Fail(error,"invalid dual momentum diagonal");
    for(const auto& edge:edges){
        if(edge.from>=n||edge.to>=n||edge.from==edge.to||!(edge.amount.Value()>=0.&&Finite(edge.amount)))return Fail(error,"invalid dual momentum edge");
        if(edge.amount.Value()>0.)rows[edge.to].push_back({edge.from,edge.amount});
    }
    next.row_.resize(n+1);next.pivot_.resize(n);
    for(std::size_t i=0;i<n;++i){
        auto& row=rows[i];row.push_back({i,Real()});std::sort(row.begin(),row.end(),[](const auto& a,const auto& b){return a.first<b.first;});
        next.row_[i]=next.entries_.size();
        for(auto [column,mass]:row){
            if(next.entries_.size()>next.row_[i]&&next.entries_.back().column==column)next.entries_.back().mass=next.entries_.back().mass+mass;
            else next.entries_.push_back({column,mass,0.});
        }
        for(std::size_t j=next.row_[i];j<next.entries_.size();++j){
            auto& e=next.entries_[j];e.scaled=e.column==i?1.:-(e.mass/diagonal[i]).Value();
            if(e.column==i)next.pivot_[i]=j;
            if(!std::isfinite(e.scaled))return Fail(error,"unrepresentable scaled momentum coefficient");
        }
    }
    next.row_[n]=next.entries_.size();for(const auto& e:next.entries_)next.factor_.push_back(e.scaled);
    for(std::size_t i=0;i<n;++i){
        for(auto a=next.row_[i];a<next.pivot_[i];++a){
            const auto j=next.entries_[a].column;const double multiplier=next.factor_[a]/next.factor_[next.pivot_[j]];
            if(!std::isfinite(multiplier))return Fail(error,"unrepresentable ILU multiplier");
            next.factor_[a]=multiplier;auto b=a+1,c=next.pivot_[j]+1;
            while(b<next.row_[i+1]&&c<next.row_[j+1]){
                auto x=next.entries_[b].column,y=next.entries_[c].column;
                if(x<y)++b;else if(y<x)++c;else{next.factor_[b]-=multiplier*next.factor_[c];++b;++c;}
            }
        }
        auto& pivot=next.factor_[next.pivot_[i]];
        if(!std::isfinite(pivot))return Fail(error,"nonfinite ILU pivot");
        // Numerical preconditioner only. Original compensated rows remain
        // unchanged and independently audited after iterative refinement.
        pivot=std::max(pivot,64.*std::numeric_limits<double>::epsilon());
    }
    *this=std::move(next);error.clear();return true;
}
void CutDualLinear::Multiply(const std::vector<double>& x,std::vector<double>& y)const{
    for(std::size_t i=0;i<x.size();++i){double value=0.;for(auto j=row_[i];j<row_[i+1];++j)value+=entries_[j].scaled*x[entries_[j].column];y[i]=value;}
}
void CutDualLinear::Precondition(const std::vector<double>& x,std::vector<double>& y)const{
    for(std::size_t i=0;i<x.size();++i){double v=x[i];for(auto j=row_[i];j<pivot_[i];++j)v-=factor_[j]*y[entries_[j].column];y[i]=v;}
    for(std::size_t i=x.size();i-->0;){double v=y[i];for(auto j=pivot_[i]+1;j<row_[i+1];++j)v-=factor_[j]*y[entries_[j].column];y[i]=v/factor_[pivot_[i]];}
}
bool CutDualLinear::Correction(const std::vector<double>& source,std::vector<double>& output)const{
    const auto n=source.size();std::vector<double> x(n),rhs=source;
    const double scale=Maximum(rhs);if(scale==0.){output=std::move(x);return true;}
    if(!std::isfinite(scale))return false;
    for(auto& v:rhs)v/=scale;
    auto residual=rhs;const double target=1e-8;
    constexpr int width=24;
    for(int restart=0;restart<20;++restart){
        const double norm=std::sqrt(Dot(residual,residual));if(!(norm>0.&&std::isfinite(norm)))return false;
        std::vector<std::vector<double>> basis(1,residual),directions;
        for(auto& v:basis[0])v/=norm;
        std::array<std::array<double,width>,width+1> h{};std::array<double,width> cosine{},sine{};std::array<double,width+1> projection{};projection[0]=norm;int used=0;
        for(int k=0;k<width;++k){
            std::vector<double> direction(n),w(n);Precondition(basis[k],direction);Multiply(direction,w);directions.push_back(std::move(direction));
            for(int pass=0;pass<2;++pass)for(int j=0;j<=k;++j){double dot=Dot(w,basis[j]);h[j][k]+=dot;for(std::size_t i=0;i<n;++i)w[i]-=dot*basis[j][i];}
            h[k+1][k]=std::sqrt(Dot(w,w));if(h[k+1][k]>0.){for(auto& v:w)v/=h[k+1][k];basis.push_back(std::move(w));}
            for(int j=0;j<k;++j){double a=h[j][k],b=h[j+1][k];h[j][k]=cosine[j]*a+sine[j]*b;h[j+1][k]=-sine[j]*a+cosine[j]*b;}
            const double length=std::hypot(h[k][k],h[k+1][k]);if(!(length>0.&&std::isfinite(length)))return false;
            cosine[k]=h[k][k]/length;sine[k]=h[k+1][k]/length;h[k][k]=length;h[k+1][k]=0.;
            projection[k+1]=-sine[k]*projection[k];projection[k]*=cosine[k];used=k+1;
            if(std::abs(projection[k+1])<target||basis.size()<=std::size_t(k+1))break;
        }
        std::array<double,width> weights{};
        for(int k=used;k-->0;){double v=projection[k];for(int j=k+1;j<used;++j)v-=h[k][j]*weights[j];weights[k]=v/h[k][k];}
        for(int k=0;k<used;++k)for(std::size_t i=0;i<n;++i)x[i]+=weights[k]*directions[k][i];
        std::vector<double> applied(n);Multiply(x,applied);for(std::size_t i=0;i<n;++i)residual[i]=rhs[i]-applied[i];
        if(Maximum(residual)<=target){for(auto& v:x)v*=scale;output=std::move(x);return true;}
    }return false;
}
bool CutDualLinear::Solve(const std::vector<Real>& rhs,std::vector<Real>& output,std::string& error)const{
    const auto n=diagonal_.size();if(!n||rhs.size()!=n)return Fail(error,"invalid dual momentum RHS dimensions");
    for(auto x:rhs)if(!Finite(x))return Fail(error,"nonfinite dual momentum RHS");
    std::vector<Real> x(n);std::vector<double> residual(n);double sourceScale=0.;
    for(std::size_t i=0;i<n;++i){double value=std::abs((rhs[i]/diagonal_[i]).Value());if(!std::isfinite(value))return Fail(error,"unrepresentable scaled momentum RHS");sourceScale=std::max(sourceScale,value);}
    for(int refinement=0;refinement<8;++refinement){
        double maximum=0.,magnitude=sourceScale;
        for(std::size_t i=0;i<n;++i){
            Real r=rhs[i]-diagonal_[i]*x[i];for(auto j=row_[i];j<row_[i+1];++j)if(entries_[j].column!=i)r=r+entries_[j].mass*x[entries_[j].column];
            residual[i]=(r/diagonal_[i]).Value();
            if(!Finite(x[i])||!std::isfinite(x[i].Value())||!std::isfinite(residual[i]))return Fail(error,"nonfinite original momentum row");
            maximum=std::max(maximum,std::abs(residual[i]));magnitude=std::max(magnitude,std::abs(x[i].Value()));
        }
        if(maximum<=2e-26*magnitude){output=std::move(x);error.clear();return true;}
        std::vector<double> correction;if(!Correction(residual,correction))return Fail(error,"dual momentum GMRES correction exhausted");
        for(std::size_t i=0;i<n;++i)x[i]=x[i]+Real(correction[i]);
    }return Fail(error,"dual momentum compensated row refinement exhausted");
}
} // namespace judas::volume_grid
