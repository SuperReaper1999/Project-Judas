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
    if(!(volume>0.&&std::isfinite(volume)&&density>0.&&std::isfinite(density)&&dt>0.&&std::isfinite(dt))||faces.empty())
        return Fail(error,"invalid liquid mimetic cell");
    std::array<Real,3> closure;std::array<std::array<Real,3>,3> moment;
    std::array<double,3> count{};std::array<Real,3> areaSum{};
    for(auto f:faces){
        if(f.axis<0||f.axis>2||(f.sign!=1&&f.sign!=-1)||!(f.area>0.&&std::isfinite(f.area)))return Fail(error,"invalid liquid boundary patch");
        for(auto x:f.offset)if(!std::isfinite(x))return Fail(error,"nonfinite local liquid geometry");
        count[f.axis]+=1.;areaSum[f.axis]=areaSum[f.axis]+f.Area();closure[f.axis]=closure[f.axis]+Real(f.sign)*f.Area();
        for(int a=0;a<3;++a)moment[f.axis][a]=moment[f.axis][a]+Real(f.sign)*f.Area()*f.Offset(a);
    }
    for(int a=0;a<3;++a){
        if(!(count[a]>0.)||std::abs(closure[a].Value())>1e-12*areaSum[a].Value())return Fail(error,"liquid patches do not enclose a volume");
        if(!std::isfinite(gravity[a]))return Fail(error,"nonfinite liquid gravity");
        for(int b=0;b<3;++b)if(std::abs((moment[a][b]-Real(a==b?volume:0.)).Value())>1e-12*volume)return Fail(error,"liquid boundary moments disagree with owned volume");
    }
    const auto n=faces.size();std::vector<Real> mass(n*n);std::vector<Real> stabilization(n);
    std::array<Real,3> stabilizationSum{};
    for(std::size_t i=0;i<n;++i){
        stabilization[i]=Real(density)*Real(volume)*faces[i].Area()/(Real(3.)*areaSum[faces[i].axis]);
        stabilizationSum[faces[i].axis]=stabilizationSum[faces[i].axis]+stabilization[i];
    }
    // Area-weighted orthogonal complement: P N = 0 and P^T S P is SPD
    // off the affine velocity space. Tiny, real boundary fragments retain
    // their own velocity without inheriting the mass of a large neighbour.
    for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j){
        Real dot;for(int a=0;a<3;++a)dot=dot+faces[i].Offset(a)*faces[j].Offset(a);
        mass[i*n+j]=Real(density)/Real(volume)*faces[i].Area()*faces[j].Area()*dot;
        if(i==j)mass[i*n+j]=mass[i*n+j]+Real(stabilization[i]);
        if(faces[i].axis==faces[j].axis)
            mass[i*n+j]=mass[i*n+j]-Real(faces[i].sign*faces[j].sign)*Real(stabilization[i])*Real(stabilization[j])/Real(stabilizationSum[faces[i].axis]);
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
        next.gravityHead[i]+=(Real(density*gravity[a])*faces[next.free[i]].Offset(a)).Value();
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
    // Form E^T M E from the generating Gram vectors, rather than
    // subtracting four nearly equal full-mass entries for a thin L/T region.
    // This is the identical compatible mass operator, not regularization.
    std::vector<std::array<Real,3>> displacement(k);
    std::vector<Real> projected(n*k);
    for(std::size_t i=0;i<k;++i){auto a=next.free[independent[i]];Real ratio=faces[a].Area()/faces[ref].Area();
        for(int d=0;d<3;++d)displacement[i][d]=faces[a].Area()*(faces[a].Offset(d)-faces[ref].Offset(d));
        for(std::size_t h=0;h<n;++h){Real weighted;
            if(faces[h].axis==faces[a].axis)weighted=weighted+Real(faces[a].sign)*stabilization[a];
            if(faces[h].axis==faces[ref].axis)weighted=weighted-ratio*Real(faces[ref].sign)*stabilization[ref];
            projected[h*k+i]=Real(h==a?1.:0.)-Real(h==ref?1.:0.)*ratio-Real(faces[h].sign)*weighted/stabilizationSum[faces[h].axis];
        }
    }
    for(std::size_t i=0;i<k;++i)for(std::size_t j=0;j<k;++j){Real value;
        for(int d=0;d<3;++d)value=value+Real(density)/Real(volume)*displacement[i][d]*displacement[j][d];
        for(std::size_t h=0;h<n;++h)value=value+stabilization[h]*projected[h*k+i]*projected[h*k+j];
        constrained[i*k+j]=value;
    }
    for(std::size_t i=0;i<k;++i){scale[i]=std::sqrt(constrained[i*k+i].Value());if(!(scale[i]>0.&&std::isfinite(scale[i])))return Fail(error,"unrepresentable constrained liquid mass");}
    bool primaryFactor=true;
    for(std::size_t i=0;i<k&&primaryFactor;++i)for(std::size_t j=0;j<=i;++j){
        Real v=constrained[i*k+j]/(Real(scale[i])*Real(scale[j]));
        for(std::size_t a=0;a<j;++a)v=v-Real(factor[i*k+a])*Real(factor[j*k+a]);
        if(i==j){if(!(v.Value()>0.&&std::isfinite(v.Value()))){primaryFactor=false;break;}factor[i*k+j]=std::sqrt(v.Value());}
        else factor[i*k+j]=v.Value()/factor[j*k+j];
    }
    auto solve=[&](const std::vector<double>& rhs){std::vector<double> x(k);
        for(std::size_t i=0;i<k;++i){double v=rhs[i]/scale[i];for(std::size_t j=0;j<i;++j)v-=factor[i*k+j]*x[j];x[i]=v/factor[i*k+i];}
        for(std::size_t i=k;i-->0;){double v=x[i];for(std::size_t j=i+1;j<k;++j)v-=factor[j*k+i]*x[j];x[i]=v/factor[i*k+i];}
        for(std::size_t i=0;i<k;++i)x[i]/=scale[i];
        return x;};
    std::vector<Real> inverse(k*k);bool admitted=primaryFactor;
    for(std::size_t column=0;column<k&&admitted;++column){
        std::vector<Real> x(k);bool converged=false;
        for(int refinement=0;refinement<8;++refinement){std::vector<double> rhs(k);double maximum=0.;
            for(std::size_t i=0;i<k;++i){Real residual(i==column?1.:0.),magnitude(i==column?1.:0.);
                for(std::size_t j=0;j<k;++j){Real term=constrained[i*k+j]*x[j];residual=residual-term;magnitude=magnitude+Real(std::abs(term.Value()));}
                rhs[i]=residual.Value();maximum=std::max(maximum,std::abs(residual.Value())/std::max(magnitude.Value(),1.));}
            if(maximum<1e-24){converged=true;break;}
            auto correction=solve(rhs);for(std::size_t i=0;i<k;++i)x[i]=x[i]+Real(correction[i]);
        }
        if(!converged){admitted=false;break;}
        for(std::size_t i=0;i<k;++i)inverse[i*k+column]=x[i];
    }
    if(!admitted){
        // Rare local fallback only. Global solves still use binary64. Retain
        // the same original inverse-row gate; never replace a failed factor
        // by a diagonal mass floor or relax the momentum/work acceptance.
        next.compensatedFactor=true;
        auto squareRoot=[](Real a){Real x(std::sqrt(a.Value()));for(int i=0;i<2;++i)x=x+(a-x*x)/(Real(2.)*x);return x;};
        // QR of the generating vectors avoids squaring their condition number
        // before factorization. B^T B is the unchanged constrained mass.
        const auto rows=n+3;std::vector<Real> qr(rows*k);
        Real geometricScale=squareRoot(Real(density)/Real(volume));
        for(std::size_t i=0;i<k;++i){
            for(int d=0;d<3;++d)qr[std::size_t(d)*k+i]=geometricScale*displacement[i][d]/Real(scale[i]);
            for(std::size_t h=0;h<n;++h)qr[(h+3)*k+i]=squareRoot(stabilization[h])*projected[h*k+i]/Real(scale[i]);
        }
        for(std::size_t column=0;column<k;++column){Real norm;
            for(std::size_t row=column;row<rows;++row)norm=norm+qr[row*k+column]*qr[row*k+column];
            if(!(norm.Value()>0.))return Fail(error,"local liquid QR has no independent kinetic direction");
            Real alpha=squareRoot(norm);if(qr[column*k+column].Value()>=0.)alpha=Real()-alpha;
            std::vector<Real> house(rows-column);for(std::size_t row=column;row<rows;++row)house[row-column]=qr[row*k+column];house[0]=house[0]-alpha;
            Real normHouse;for(auto v:house)normHouse=normHouse+v*v;
            for(std::size_t j=column+1;j<k;++j){Real dot;for(std::size_t row=column;row<rows;++row)dot=dot+house[row-column]*qr[row*k+j];Real coefficient=Real(2.)*dot/normHouse;for(std::size_t row=column;row<rows;++row)qr[row*k+j]=qr[row*k+j]-coefficient*house[row-column];}
            qr[column*k+column]=alpha;for(std::size_t row=column+1;row<rows;++row)qr[row*k+column]={};
        }
        auto preciseSolve=[&](const std::vector<Real>& rhs){std::vector<Real> x(k);
            for(std::size_t i=0;i<k;++i){Real v=rhs[i]/Real(scale[i]);for(std::size_t j=0;j<i;++j)v=v-qr[j*k+i]*x[j];x[i]=v/qr[i*k+i];}
            for(std::size_t i=k;i-->0;){Real v=x[i];for(std::size_t j=i+1;j<k;++j)v=v-qr[i*k+j]*x[j];x[i]=v/qr[i*k+i];}
            for(std::size_t i=0;i<k;++i)x[i]=x[i]/Real(scale[i]);
            return x;
        };
        for(std::size_t column=0;column<k;++column){std::vector<Real> x(k);bool converged=false;
            for(int refinement=0;refinement<8;++refinement){std::vector<Real> rhs(k);double maximum=0.;
                for(std::size_t i=0;i<k;++i){Real residual(i==column?1.:0.),magnitude(i==column?1.:0.);for(std::size_t j=0;j<k;++j){Real term=constrained[i*k+j]*x[j];residual=residual-term;magnitude=magnitude+Real(std::abs(term.Value()));}rhs[i]=residual;maximum=std::max(maximum,std::abs(residual.Value())/std::max(magnitude.Value(),1.));}
                if(maximum<1e-24){converged=true;break;}auto correction=preciseSolve(rhs);for(std::size_t i=0;i<k;++i)x[i]=x[i]+correction[i];
            }
            if(!converged)return Fail(error,"compensated liquid inverse failed the unchanged original-row gate");
            for(std::size_t i=0;i<k;++i)inverse[i*k+column]=x[i];
        }

    }
    for(std::size_t i=0;i<k;++i)for(std::size_t j=0;j<k;++j){auto a=independent[i],b=independent[j];next.mobility[a*m+b]=faces[next.free[a]].Area()*inverse[i*k+j]*faces[next.free[b]].Area();}
    std::vector<Real> driving(m);
    for(std::size_t i=0;i<m;++i){
        auto f=faces[next.free[i]];Real dot;
        for(int a=0;a<3;++a)dot=dot+f.Offset(a)*Real(sourceVelocity[a]);
        driving[i]=Real(density)*dot;
    }
    for(auto i:independent){Real q;
        for(auto j:independent)q=q+next.mobility[i*m+j]*(driving[j]-driving[next.reference]);
        next.predictor[i]=q;
    }
    Real sumPredictor;for(auto i:independent)sumPredictor=sumPredictor+next.predictor[i];next.predictor[next.reference]=Real()-sumPredictor;
    for(auto i:independent){Real sum;for(auto j:independent)sum=sum+next.mobility[i*m+j];next.mobility[i*m+next.reference]=Real()-sum;next.mobility[next.reference*m+i]=Real()-sum;}
    Real sum;for(auto i:independent)sum=sum-Real(next.mobility[next.reference*m+i]);next.mobility[next.reference*m+next.reference]=sum;
    output=std::move(next);error.clear();return true;
}
}
