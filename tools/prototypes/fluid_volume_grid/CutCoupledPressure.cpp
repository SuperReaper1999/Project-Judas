#include "CutCoupledResidual.h"
#include "CutPressurePreconditioner.h"
#include "CutCoupledKrylov.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>

namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Fail(std::string& e,const char* s){e=s;return false;}
double Dot(const std::vector<double>& a,const std::vector<double>& b){long double sum=0.;for(std::size_t i=0;i<a.size();++i)sum+=static_cast<long double>(a[i])*b[i];return double(sum);}
bool Orthogonal(const CutSpatialField& f){
    for(auto face:f.geometry.faces)if(face.meanArea>0.)for(int a=0;a<3;++a)if(a!=face.axis){
        const auto& l=f.chart.regions[face.left];const auto& r=f.chart.regions[face.right];
        if(l.lo[a]!=r.lo[a]||l.hi[a]!=r.hi[a])return false;
    }return true;
}
}
bool AdvanceCutCoupledPressure(const CutSpatialField& field,const CutVelocityDual& dual,
    const CutDynamicSettings& settings,const CutCoupledSettings& options,CutCoupledState& output,
    std::string& error,const CutCoupledState* previous,const std::vector<double>& seed,const CutCoupledInitialGuess* initialGuess){
    const auto n=field.parent.size(),nu=dual.Nodes().size();
    if(previous==&output||options.iterationLimit<1||!(options.momentumTolerance>0.&&std::isfinite(options.momentumTolerance))||
        !n||!dual.Matches(field)||field.chart.regions.size()!=n||field.geometry.startCapacity.size()!=n||
        field.water.size()!=n||field.gasMass.size()!=n||(!seed.empty()&&seed.size()!=n)||
        !(settings.dt>0.&&std::isfinite(settings.dt)&&settings.waterDensity>0.&&std::isfinite(settings.waterDensity)&&settings.gasDensity>0.&&std::isfinite(settings.gasDensity)&&
        settings.soundSpeed>0.&&std::isfinite(settings.soundSpeed*settings.soundSpeed)&&settings.volumeTolerance>0.&&std::isfinite(settings.volumeTolerance)&&
        settings.relativeVolumeTolerance>0.&&settings.relativeVolumeTolerance<1.))return Fail(error,"invalid coupled pressure state or options");
    for(double g:settings.gravity)if(!std::isfinite(g))return Fail(error,"nonfinite coupled gravity");
    if(options.linearization!=CutCoupledLinearization::Newton&&options.linearization!=CutCoupledLinearization::Oseen)return Fail(error,"invalid coupled linearization");
    if(!Orthogonal(field))return Fail(error,"coupled normal-pressure operator requires a conforming orthogonal chart");
    if(field.geometry.motion)return Fail(error,"coupled momentum currently requires precompiled frozen solids");
    auto stamp=CutLocalGeometryStamp(field);
    if(previous&&(previous->material.frozenGeometryStamp!=stamp||previous->material.waterDensity!=settings.waterDensity||
        previous->material.gasDensity!=settings.gasDensity||previous->material.soundSpeed!=settings.soundSpeed||
        previous->velocity.size()!=nu||previous->material.water.size()!=n||previous->material.gasMass.size()!=n||previous->material.pressure.size()!=n))
        return Fail(error,"coupled continuation geometry, law or momentum ownership changed");
    const auto started=std::chrono::steady_clock::now();
    const auto sw=previous?previous->material.water:std::vector<Real>(field.water.begin(),field.water.end());
    const auto sm=previous?previous->material.gasMass:std::vector<Real>(field.gasMass.begin(),field.gasMass.end());
    const auto su=previous?previous->velocity:std::vector<Real>(nu);
    if(previous)for(std::size_t i=0;i<n;++i){
        Real rho=Real(settings.gasDensity)+previous->material.pressure[i]/Real(settings.soundSpeed*settings.soundSpeed);
        double tolerance=std::min(settings.volumeTolerance,settings.relativeVolumeTolerance*field.geometry.startCapacity[i]);
        if(!(rho.Value()>0.)||std::abs((Real(field.geometry.startCapacity[i])-sw[i]-sm[i]/rho).Value())>tolerance)
            return Fail(error,"coupled continuation source exceeds original volume tolerance");
    }
    std::vector<Real> p=previous?previous->material.pressure:std::vector<Real>(n),u=su;
    for(std::size_t i=0;i<seed.size();++i)p[i]=seed[i];
    // Start Newton from a momentum predictor, not the nondifferentiable
    // zero-flux corner of every cell at once. This is only an iterate; the
    // admitted step still solves the original full coupled residual.
    std::vector<Real> sourceMass(n);for(std::size_t i=0;i<n;++i)sourceMass[i]=Real(settings.waterDensity)*sw[i]+sm[i];
    CutDualMomentum predictor;
    if(!predictor.Build(dual,field,settings,sourceMass,sourceMass,std::vector<Real>(field.geometry.faces.size()),
        std::vector<Real>(field.boundaries.size()),su,error,options.momentumBackend))return false;
    std::vector<Real> predictedQ,predictedPort;
    if(!predictor.Flow(p,predictedQ,predictedPort,error,&u))return false;
    if(initialGuess){
        if(initialGuess->pressure.size()!=n||initialGuess->velocity.size()!=nu)return Fail(error,"coupled initial guess dimensions mismatch");
        p=initialGuess->pressure;u=initialGuess->velocity;
    }
    auto current=std::make_unique<CutCoupledResidual>();
    if(!current->Build(field,dual,settings,sw,sm,su,p,u,options.momentumBackend,error))return false;
    std::vector<double> sorted=field.geometry.startCapacity;std::sort(sorted.begin(),sorted.end());
    std::vector<bool> small(n);for(std::size_t i=0;i<n;++i)small[i]=sorted[n/2]*.125>field.geometry.startCapacity[i];
    for(int iteration=0;iteration<options.iterationLimit;++iteration){
        std::cout<<"JOINT iteration="<<iteration<<" volume="<<current->material.maximumNormalizedResidual<<" momentum="<<current->audit.maximumNormalizedResidual<<" speed="<<current->audit.maximumSpeed<<std::endl;
        if(current->material.maximumNormalizedResidual<=1.&&current->audit.maximumNormalizedResidual<=options.momentumTolerance){
            if(!CutLocalDryPorts(field,current->material.water,current->material.gasMass,p,settings,error))return false;
            Real tw,tm,iw,im;
            for(std::size_t i=0;i<n;++i){
                double tol=std::min(settings.volumeTolerance,settings.relativeVolumeTolerance*field.geometry.startCapacity[i]);
                if(current->material.water[i].Value()<0.||current->material.gasMass[i].Value()<0.||
                    (current->material.water[i]-Real(field.geometry.startCapacity[i])).Value()>tol)return Fail(error,"joint phase admission failed");
                tw=tw+current->material.water[i];tm=tm+current->material.gasMass[i];iw=iw+sw[i];im=im+sm[i];
            }
            CutSelectiveTransport ledger;std::vector<CutTransportFace> faces;std::vector<CutTransportGasPort> ports;
            for(std::size_t f=0;f<field.geometry.faces.size();++f)faces.push_back({field.geometry.faces[f].left,field.geometry.faces[f].right,current->material.flux[f]});
            for(std::size_t j=0;j<field.boundaries.size();++j)ports.push_back({field.boundaries[j].fragment,current->material.portFlux[j]});
            if(!ledger.Build(field.geometry.startCapacity,faces,ports,sw,sm,settings.gasDensity,settings.volumeTolerance*1e-4,error))return false;
            for(auto m:ledger.GasPortMass()){tm=tm+m;current->material.externalGasMass=current->material.externalGasMass+m;}
            if(std::abs((tw-iw).Value())>settings.volumeTolerance||std::abs((tm-im).Value())>settings.gasDensity*settings.volumeTolerance)return Fail(error,"joint original quantity ledger failed");
            auto a=current->audit;double workScale=std::abs(a.beforeEnergy)+std::abs(a.afterEnergy)+std::abs(a.pressureWork)+std::abs(a.gravityWork)+a.unsteadyLoss+std::abs(a.mixingLoss);
            if(std::abs(a.energyResidual)>1e-9*std::max(1.,workScale))return Fail(error,"joint kinetic work audit failed");
            CutCoupledState next;next.material=std::move(current->material);next.material.dt=settings.dt;next.material.waterDensity=settings.waterDensity;
            next.material.gasDensity=settings.gasDensity;next.material.soundSpeed=settings.soundSpeed;next.material.frozenGeometryStamp=stamp;next.material.iterations=iteration+1;
            next.velocity=std::move(u);next.momentum=a;next.nonlinearIterations=iteration+1;
            next.elapsedMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();output=std::move(next);error.clear();return true;
        }
        const auto& free=current->momentum.FreeNodes();const auto total=n+free.size();
        std::vector<double> storage(n),rowScale(n),momentumScale(nu);
        std::vector<CutHydroacousticGraphLink> links;
        for(std::size_t i=0;i<n;++i){Real rho=Real(settings.gasDensity)+p[i]/Real(settings.soundSpeed*settings.soundSpeed);storage[i]=(sm[i]/(rho*rho*Real(settings.soundSpeed*settings.soundSpeed))).Value();}
        const auto& fg=current->momentum.FaceConductance();const auto& pg=current->momentum.PortConductance();
        for(std::size_t f=0;f<fg.size();++f)if(fg[f]>0.)links.push_back({field.geometry.faces[f].left,field.geometry.faces[f].right,fg[f]});
        for(std::size_t j=0;j<pg.size();++j)storage[field.boundaries[j].fragment]+=pg[j];
        rowScale=storage;for(auto e:links){rowScale[e.left]+=e.conductance;rowScale[e.right]+=e.conductance;}
        const double pressureUnit=1.;
        for(auto& x:rowScale)x=std::max(1e-300,x*pressureUnit);
        for(auto j:free)momentumScale[j]=current->momentum.Diagonal()[j].Value();
        CutPressurePreconditioner pressure;
        if(!pressure.Build(storage,links,small,error))return false;
        auto flatten=[&](const std::vector<Real>& v,const std::vector<Real>& m){std::vector<double> r(total);for(std::size_t i=0;i<n;++i)r[i]=v[i].Value()/rowScale[i];for(std::size_t k=0;k<free.size();++k)r[n+k]=m[free[k]].Value()/momentumScale[free[k]];return r;};
        auto precondition=[&](const std::vector<double>& r,std::vector<double>& result){
            std::vector<Real> impulse(nu),du0;for(std::size_t k=0;k<free.size();++k)impulse[free[k]]=Real(r[n+k]*momentumScale[free[k]]);
            if(!current->momentum.SolveImpulse(impulse,du0,error))return false;
            std::vector<Real> rhs(n);for(std::size_t i=0;i<n;++i)rhs[i]=Real(r[i]*rowScale[i]);
            for(auto j:free){auto node=dual.Nodes()[j];Real flux=Real(settings.dt*node.area)*du0[j];
                if(node.face!=SIZE_MAX){auto f=field.geometry.faces[node.face];rhs[f.left]=rhs[f.left]-flux;rhs[f.right]=rhs[f.right]+flux;}
                else{auto c=field.boundaries[node.port].fragment;rhs[c]=rhs[c]-Real(node.sign)*flux;}
            }
            std::vector<double> pressureRhs(n),dp;for(std::size_t i=0;i<n;++i)pressureRhs[i]=rhs[i].Value();
            if(!pressure.Apply(pressureRhs,dp,error))return false;
            auto correction=current->momentum.PressureImpulse(std::vector<Real>(dp.begin(),dp.end()));std::vector<Real> du;
            for(auto j:free)correction[j]=correction[j]+impulse[j];
            if(!current->momentum.SolveImpulse(correction,du,error))return false;
            result=dp;for(auto j:free)result.push_back(du[j].Value());return true;
        };
        auto apply=[&](const std::vector<double>& direction,std::vector<double>& applied){std::vector<double> dp(direction.begin(),direction.begin()+n),du(nu);
            for(std::size_t k=0;k<free.size();++k)du[free[k]]=direction[n+k];
            std::vector<Real> vr,mr;
            if(!current->Tangent(dp,du,vr,mr,error))return false;
            // Oseen linearization only: update actual mass and carriers at
            // every nonlinear trial. Publication still requires full rows.
            if(options.linearization==CutCoupledLinearization::Oseen)mr=current->momentum.FrozenTangent(dp,du);
            applied=flatten(vr,mr);return true;
        };
        auto rhs=flatten(current->volumeRows,current->momentumRows);for(auto& x:rhs)x=-x;
        double norm=std::sqrt(Dot(rhs,rhs));if(!(norm>0.&&std::isfinite(norm)))return Fail(error,"invalid joint residual norm");
        CutKrylovStats linear;std::vector<Real> direction;
        if(!SolveCutCoupledKrylov(rhs,apply,precondition,direction,linear,error))return false;
        std::vector<Real> dp(direction.begin(),direction.begin()+n),du(nu);
        for(std::size_t j=0;j<free.size();++j)du[free[j]]=direction[n+j];
        double line=1.;for(std::size_t i=0;i<n;++i)if(dp[i].Value()<0.){Real rho=Real(settings.gasDensity)+p[i]/Real(settings.soundSpeed*settings.soundSpeed);line=std::min(line,(Real(.9)*rho*Real(settings.soundSpeed*settings.soundSpeed)/(Real()-dp[i])).Value());}
        bool accepted=false;std::string rejected;
        for(int backtrack=0;backtrack<28;++backtrack){auto trialP=p,trialU=u;for(std::size_t i=0;i<n;++i)trialP[i]=trialP[i]+dp[i]*Real(line);for(auto j:free)trialU[j]=trialU[j]+du[j]*Real(line);
            auto trial=std::make_unique<CutCoupledResidual>();
            if(trial->Build(field,dual,settings,sw,sm,su,trialP,trialU,options.momentumBackend,rejected)){
                auto r=flatten(trial->volumeRows,trial->momentumRows);double merit=std::sqrt(Dot(r,r));
                if(merit<norm*(1.-1e-4*line)){current=std::move(trial);p=std::move(trialP);u=std::move(trialU);accepted=true;break;}
            }
            line*=.5;
        }
        std::cout<<"JOINT_STEP krylov="<<linear.applications<<" predicted="<<linear.relativeResidual<<" line="<<line<<std::endl;
        if(!accepted){error="joint pressure/momentum line search exhausted (linear relative residual="+std::to_string(linear.relativeResidual)+"): "+rejected;return false;}
    }
    return Fail(error,"joint pressure/momentum nonlinear budget exhausted");
}
} // namespace judas::volume_grid
