#include "WaterMimeticCell.h"
#include "CutCondensedPressure.h"
#include <algorithm>
#include <cmath>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Fail(std::string& e,const char* why){e=why;return false;}
}
bool PrepareWaterMimeticCell(double volume,double density,double dt,const GridVec& gravity,
    const std::vector<WaterCellPatch>& faces,WaterCellProjection& output,std::string& error,const GridVec& sourceVelocity){
    if(!(volume>0.&&std::isfinite(volume)&&density>0.&&std::isfinite(density)&&dt>0.&&std::isfinite(dt))||faces.empty()||faces.size()>32)
        return Fail(error,"invalid liquid mimetic cell");
    std::array<Real,3> closure;std::array<std::array<Real,3>,3> moment;
    std::array<double,3> count{},areaSum{};
    for(auto f:faces){
        if(f.axis<0||f.axis>2||(f.sign!=1&&f.sign!=-1)||!(f.area>0.&&std::isfinite(f.area)))return Fail(error,"invalid liquid boundary patch");
        for(auto x:f.offset)if(!std::isfinite(x))return Fail(error,"nonfinite local liquid geometry");
        count[f.axis]+=1.;areaSum[f.axis]+=f.area;closure[f.axis]=closure[f.axis]+Real(f.sign)*Real(f.area);
        for(int a=0;a<3;++a)moment[f.axis][a]=moment[f.axis][a]+Real(f.sign)*Real(f.area)*Real(f.offset[a]);
    }
    for(int a=0;a<3;++a){
        if(!(count[a]>0.)||std::abs(closure[a].Value())>1e-12*areaSum[a])return Fail(error,"liquid patches do not enclose a volume");
        if(!std::isfinite(gravity[a]))return Fail(error,"nonfinite liquid gravity");
        for(int b=0;b<3;++b)if(std::abs((moment[a][b]-Real(a==b?volume:0.)).Value())>1e-12*volume)return Fail(error,"liquid boundary moments disagree with owned volume");
    }
    const auto n=faces.size();std::vector<double> mass(n*n),stabilization(n);
    std::array<double,3> stabilizationSum{};
    for(std::size_t i=0;i<n;++i){
        stabilization[i]=density*volume*faces[i].area/(3.*areaSum[faces[i].axis]);
        stabilizationSum[faces[i].axis]+=stabilization[i];
    }
    // Area-weighted orthogonal complement: P N = 0 and P^T S P is SPD
    // off the affine velocity space. Tiny, real boundary fragments retain
    // their own velocity without inheriting the mass of a large neighbour.
    for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j){
        double dot=0.;for(int a=0;a<3;++a)dot+=faces[i].offset[a]*faces[j].offset[a];
        mass[i*n+j]=density/volume*faces[i].area*faces[j].area*dot;
        if(i==j)mass[i*n+j]+=stabilization[i];
        if(faces[i].axis==faces[j].axis)
            mass[i*n+j]-=faces[i].sign*faces[j].sign*stabilization[i]*stabilization[j]/stabilizationSum[faces[i].axis];
    }
    WaterCellProjection next;next.mass=mass;next.sourceVelocity=sourceVelocity;
    for(double v:sourceVelocity)if(!std::isfinite(v))return Fail(error,"nonfinite liquid source velocity");
    for(std::size_t i=0;i<n;++i)if(!faces[i].fixed)next.free.push_back(i);
    const auto m=next.free.size();
    if(!m){output=std::move(next);error.clear();return true;}
    next.reference=0;
    for(std::size_t i=1;i<m;++i)if(faces[next.free[i]].area>faces[next.free[next.reference]].area)next.reference=i;
    next.mobility.assign(m*m,0.);next.predictor.assign(m,0.);next.gravityHead.resize(m);
    for(std::size_t i=0;i<m;++i)for(int a=0;a<3;++a)
        next.gravityHead[i]+=density*gravity[a]*faces[next.free[i]].offset[a];
    const auto ref=next.free[next.reference];
    std::vector<std::size_t> independent;
    for(std::size_t i=0;i<m;++i)if(i!=next.reference)independent.push_back(i);
    const auto k=independent.size();
    if(!k){output=std::move(next);error.clear();return true;}
    // Eliminate divergence exactly in velocity coordinates before inversion.
    // E has one independent velocity per non-reference patch and
    // E_ref,j = -A_j/A_ref. It never forms D - D1(1^T D)/(1^T D1),
    // whose cancellation corrupts sub-ULP-thin, nearly sealed cells.
    std::vector<Real> constrained(k*k);std::vector<double> scale(k),factor(k*k);
    for(std::size_t i=0;i<k;++i)for(std::size_t j=0;j<k;++j){
        auto a=next.free[independent[i]],b=next.free[independent[j]];
        Real ar=Real(faces[a].area)/Real(faces[ref].area),br=Real(faces[b].area)/Real(faces[ref].area);
        constrained[i*k+j]=(Real(mass[a*n+b])-ar*Real(mass[ref*n+b])-br*Real(mass[a*n+ref])+ar*br*Real(mass[ref*n+ref]));
    }
    for(std::size_t i=0;i<k;++i){scale[i]=std::sqrt(constrained[i*k+i].Value());if(!(scale[i]>0.&&std::isfinite(scale[i])))return Fail(error,"unrepresentable constrained liquid mass");}
    for(std::size_t i=0;i<k;++i)for(std::size_t j=0;j<=i;++j){
        Real v=constrained[i*k+j]/(Real(scale[i])*Real(scale[j]));
        for(std::size_t a=0;a<j;++a)v=v-Real(factor[i*k+a])*Real(factor[j*k+a]);
        if(i==j){if(!(v.Value()>0.&&std::isfinite(v.Value())))return Fail(error,"constrained liquid mass is not positive definite");factor[i*k+j]=std::sqrt(v.Value());}
        else factor[i*k+j]=v.Value()/factor[j*k+j];
    }
    auto solve=[&](const std::vector<double>& rhs){std::vector<double> x(k);
        for(std::size_t i=0;i<k;++i){double v=rhs[i]/scale[i];for(std::size_t j=0;j<i;++j)v-=factor[i*k+j]*x[j];x[i]=v/factor[i*k+i];}
        for(std::size_t i=k;i-->0;){double v=x[i];for(std::size_t j=i+1;j<k;++j)v-=factor[j*k+i]*x[j];x[i]=v/factor[i*k+i];}
        for(std::size_t i=0;i<k;++i)x[i]/=scale[i];
        return x;};
    for(std::size_t column=0;column<k;++column){
        std::vector<Real> x(k);bool converged=false;
        for(int refinement=0;refinement<8;++refinement){std::vector<double> rhs(k);double maximum=0.;
            for(std::size_t i=0;i<k;++i){Real residual(i==column?1.:0.),magnitude(i==column?1.:0.);
                for(std::size_t j=0;j<k;++j){Real term=constrained[i*k+j]*x[j];residual=residual-term;magnitude=magnitude+Real(std::abs(term.Value()));}
                rhs[i]=residual.Value();maximum=std::max(maximum,std::abs(residual.Value())/std::max(magnitude.Value(),1.));}
            if(maximum<1e-24){converged=true;break;}
            auto correction=solve(rhs);for(std::size_t i=0;i<k;++i)x[i]=x[i]+Real(correction[i]);
        }
        if(!converged)return Fail(error,"constrained liquid inverse failed original-row refinement");
        for(std::size_t i=0;i<k;++i){auto a=independent[i],b=independent[column];next.mobility[a*m+b]=Real(faces[next.free[a]].area)*x[i]*Real(faces[next.free[b]].area);}
    }
    std::vector<Real> driving(m);
    for(std::size_t i=0;i<m;++i){
        auto f=faces[next.free[i]];Real dot;
        for(int a=0;a<3;++a)dot=dot+Real(f.offset[a])*Real(sourceVelocity[a]);
        driving[i]=Real(density)*dot;
    }
    for(auto i:independent){Real q;
        for(auto j:independent)q=q+next.mobility[i*m+j]*(driving[j]-driving[next.reference]);
        next.predictor[i]=q.Value();
    }
    Real sumPredictor;for(auto i:independent)sumPredictor=sumPredictor+Real(next.predictor[i]);next.predictor[next.reference]=-sumPredictor.Value();
    for(auto i:independent){Real sum;for(auto j:independent)sum=sum+next.mobility[i*m+j];next.mobility[i*m+next.reference]=Real()-sum;next.mobility[next.reference*m+i]=Real()-sum;}
    Real sum;for(auto i:independent)sum=sum-Real(next.mobility[next.reference*m+i]);next.mobility[next.reference*m+next.reference]=sum;
    output=std::move(next);error.clear();return true;
}
}
