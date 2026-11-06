#include "WaterGasProjection.h"
#include "CutCoupledKrylov.h"
#include "CutPressurePreconditioner.h"
#include "WaterPressureILU.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Fail(std::string& e,const char* why){e=why;return false;}
double Norm(const std::vector<double>& x){long double s=0.;for(double a:x)s+=static_cast<long double>(a)*a;return std::sqrt(double(s));}
}
bool ProjectWaterGasPressure(const WaterMimeticGeometry& water,const WaterGasGeometry& gas,
    const WaterGasSettings& settings,WaterGasProjectionResult& output,std::string& error,const WaterGasProjectionResult* seed,int limit){
    const auto nw=water.sharedFaces,ng=gas.volume.size(),n=nw+ng;
    for(auto l:gas.links)if(l.left>=ng||l.right>=ng||l.left==l.right||!(l.massConductance>0.&&std::isfinite(l.massConductance)))return Fail(error,"invalid liquid/gas finite connection");
    for(auto v:gas.vents)if(v.node>=ng||!(v.massConductance>0.&&std::isfinite(v.massConductance)&&v.gaugePressure>-settings.referencePressure&&std::isfinite(v.gaugePressure)))return Fail(error,"invalid liquid/gas atmospheric port");
    if(!n||gas.mass.size()!=ng||gas.containingCapacity.size()!=ng||!(settings.dt>0.&&std::isfinite(settings.dt)&&settings.referencePressure>0.&&std::isfinite(settings.referencePressure)&&settings.referenceDensity>0.&&std::isfinite(settings.referenceDensity))||!(settings.massTolerance>0.&&std::isfinite(settings.massTolerance)&&settings.relativeTolerance>0.&&settings.relativeTolerance<1.)||limit<1)
        return Fail(error,"invalid liquid/gas projection");
    for(std::size_t g=0;g<ng;++g)if(!(gas.volume[g].Value()>0.&&gas.mass[g].Value()>0.&&gas.containingCapacity[g].Value()>0.&&std::isfinite(gas.volume[g].Value())&&std::isfinite(gas.mass[g].Value())))return Fail(error,"invalid independently owned gas quantity");
    for(const auto& r:water.regions){
        if(r.sourceCell>=water.cellCapacity.size()||r.potential.size()!=r.patches.size()||r.owners.size()!=r.patches.size()||r.response.mobility.size()!=r.response.free.size()*r.response.free.size()||r.response.predictor.size()!=r.response.free.size()||(!r.response.free.empty()&&r.response.reference>=r.response.free.size()))return Fail(error,"liquid pressure geometry size mismatch");
        for(auto patch:r.response.free){
            if(patch>=r.owners.size())return Fail(error,"liquid pressure patch index is invalid");
            auto o=r.owners[patch];if((o.shared==SIZE_MAX)==(o.gas==SIZE_MAX)||(o.shared!=SIZE_MAX&&o.shared>=nw)||(o.gas!=SIZE_MAX&&o.gas>=ng))return Fail(error,"liquid pressure patch requires exactly one valid pressure owner");
        }
    }
    auto started=std::chrono::steady_clock::now();Real datum;bool found=false;
    for(const auto& r:water.regions)for(auto j:r.response.free)if(r.owners[j].gas!=SIZE_MAX&&!found){datum=r.potential[j];found=true;}
    std::vector<double> diagonal(n),tolerance(n,settings.massTolerance/settings.referenceDensity);
    for(const auto& r:water.regions){auto m=r.response.free.size();
        for(std::size_t a=0;a<m;++a){auto patch=r.response.free[a];auto owner=r.owners[patch];auto i=owner.shared!=SIZE_MAX?owner.shared:nw+owner.gas;
            if(i>=n)return Fail(error,"liquid projection patch has no pressure owner");
            diagonal[i]+=(Real(settings.dt*settings.dt)*r.response.mobility[a*m+a]).Value();
            tolerance[i]=std::min(tolerance[i],settings.relativeTolerance*water.cellCapacity[r.sourceCell]);}
    }
    for(std::size_t g=0;g<ng;++g)diagonal[nw+g]+=gas.volume[g].Value()/settings.referencePressure;
    for(auto l:gas.links){double d=settings.dt*l.massConductance/settings.referenceDensity;diagonal[nw+l.left]+=d;diagonal[nw+l.right]+=d;}
    for(auto v:gas.vents)diagonal[nw+v.node]+=settings.dt*v.massConductance/settings.referenceDensity;
    for(std::size_t i=0;i<n;++i){if(!(diagonal[i]>0.&&std::isfinite(diagonal[i])))return Fail(error,"liquid pressure component needs a physical boundary or gauge");}
    for(std::size_t i=0;i<ng;++i)tolerance[nw+i]=std::min(tolerance[nw+i],settings.relativeTolerance*gas.containingCapacity[i].Value());
    std::vector<double> storage(n);std::vector<CutHydroacousticGraphLink> edges;
    for(const auto& r:water.regions){auto m=r.response.free.size();
        for(std::size_t a=0;a<m;++a)for(std::size_t b=a+1;b<m;++b){
            auto oa=r.owners[r.response.free[a]],ob=r.owners[r.response.free[b]];
            auto i=oa.shared!=SIZE_MAX?oa.shared:nw+oa.gas,j=ob.shared!=SIZE_MAX?ob.shared:nw+ob.gas;
            double conductance=(Real(-.5*settings.dt*settings.dt)*(r.response.mobility[a*m+b]+r.response.mobility[b*m+a])).Value();
            if(i!=j&&conductance>0.)edges.push_back({i,j,conductance});
        }
    }
    for(std::size_t g=0;g<ng;++g)storage[nw+g]=gas.volume[g].Value()/settings.referencePressure;
    for(auto l:gas.links)edges.push_back({nw+l.left,nw+l.right,settings.dt*l.massConductance/settings.referenceDensity});
    for(auto v:gas.vents)storage[nw+v.node]+=settings.dt*v.massConductance/settings.referenceDensity;
    CutPressurePreconditioner factor;
    const auto liquidEdges=edges.size()-gas.links.size();
    struct Evaluation{std::vector<Real> rows,mass,volume,density;std::vector<std::vector<Real>> q,difference;std::vector<Real> referencePressure;};
    auto evaluate=[&](const std::vector<Real>& x,Evaluation& out){
        Evaluation next;next.rows.resize(n);next.mass=gas.mass;next.volume=gas.volume;next.density.resize(ng);
        for(std::size_t g=0;g<ng;++g){next.density[g]=Real(settings.referenceDensity)*x[nw+g]/Real(settings.referencePressure);if(!(next.density[g].Value()>0.))return false;}
        for(const auto& r:water.regions){auto m=r.response.free.size();std::vector<Real> h(m),q(m);
            if(!m){next.q.push_back({});next.difference.push_back({});next.referencePressure.push_back({});continue;}
            for(std::size_t a=0;a<m;++a){auto p=r.response.free[a];auto o=r.owners[p];h[a]=o.shared!=SIZE_MAX?x[o.shared]:x[nw+o.gas]-Real(settings.referencePressure)+r.potential[p]-datum;}
            std::vector<Real> difference(m);auto refPatch=r.response.free[r.response.reference];auto refOwner=r.owners[refPatch];
            for(std::size_t b=0;b<m;++b){auto patch=r.response.free[b];auto owner=r.owners[patch];difference[b]=h[b]-h[r.response.reference];
                if(owner.gas!=SIZE_MAX&&refOwner.gas!=SIZE_MAX){difference[b]=x[nw+owner.gas]-x[nw+refOwner.gas];for(int axis=0;axis<3;++axis)difference[b]=difference[b]-Real(water.density*water.gravity[axis])*(r.patches[patch].Offset(axis)-r.patches[refPatch].Offset(axis));}}
            for(std::size_t a=0;a<m;++a)if(a!=r.response.reference){q[a]=r.response.predictor[a];for(std::size_t b=0;b<m;++b)if(b!=r.response.reference)q[a]=q[a]-Real(settings.dt*r.response.mobility[a*m+b])*difference[b];}
            next.difference.push_back(difference);next.referencePressure.push_back(refOwner.gas!=SIZE_MAX?x[nw+refOwner.gas]-Real(settings.referencePressure):x[refOwner.shared]-r.potential[refPatch]+datum);
            for(std::size_t a=0;a<m;++a)if(a!=r.response.reference)q[r.response.reference]=q[r.response.reference]-q[a];
            for(std::size_t a=0;a<m;++a){
                auto o=r.owners[r.response.free[a]];if(o.shared!=SIZE_MAX)next.rows[o.shared]=next.rows[o.shared]-Real(settings.dt)*q[a];
                else next.volume[o.gas]=next.volume[o.gas]-Real(settings.dt)*q[a];}
            next.q.push_back(std::move(q));
        }
        for(auto l:gas.links){Real mass=Real(settings.dt*l.massConductance)*(x[nw+l.left]-x[nw+l.right]);next.mass[l.left]=next.mass[l.left]-mass;next.mass[l.right]=next.mass[l.right]+mass;}
        for(auto v:gas.vents){Real mass=Real(settings.dt*v.massConductance)*(x[nw+v.node]-Real(settings.referencePressure)-Real(v.gaugePressure));next.mass[v.node]=next.mass[v.node]-mass;}
        for(std::size_t g=0;g<ng;++g)next.rows[nw+g]=next.volume[g]-next.mass[g]/next.density[g];
        out=std::move(next);return true;
    };
    auto normalize=[&](const std::vector<Real>& rows){std::vector<double> result(n);for(std::size_t i=0;i<n;++i)result[i]=rows[i].Value()/diagonal[i];return result;};
    std::vector<Real> x(n);std::vector<bool> seeded(nw);bool level=true;
    for(const auto& r:water.regions)for(auto p:r.response.free)if(r.owners[p].gas!=SIZE_MAX&&(r.potential[p]-datum).Value()!=0.)level=false;
    for(const auto& r:water.regions)for(auto p:r.response.free){auto owner=r.owners[p];if(owner.shared!=SIZE_MAX&&!seeded[owner.shared]){x[owner.shared]=level?Real():r.potential[p]-datum;seeded[owner.shared]=true;}}
    for(std::size_t g=0;g<ng;++g)x[nw+g]=Real(settings.referencePressure);
    if(seed){
        if(seed->head.size()!=nw||seed->gasAbsolutePressure.size()!=ng)return Fail(error,"pressure initial guess dimensions do not match the current geometry");
        for(std::size_t i=0;i<nw;++i){x[i]=seed->head[i]+seed->headDatum-datum;if(!std::isfinite(x[i].Value()))return Fail(error,"pressure initial guess is not finite");}
        for(std::size_t g=0;g<ng;++g){x[nw+g]=seed->gasAbsolutePressure[g];if(!(x[nw+g].Value()>0.&&std::isfinite(x[nw+g].Value())))return Fail(error,"air pressure initial guess is not positive finite");}
    }
    Evaluation current;if(!evaluate(x,current))return Fail(error,"liquid projection initial pressure invalid");int applications=0,fallbacks=0;
    for(int iteration=0;iteration<limit;++iteration){double maximum=0.;std::size_t largest=0;Real aggregate;for(std::size_t i=0;i<n;++i){aggregate=aggregate+current.rows[i];double ratio=std::abs(current.rows[i].Value())/tolerance[i];if(ratio>maximum){maximum=ratio;largest=i;}}maximum=std::max(maximum,std::abs(aggregate.Value())/(settings.massTolerance/settings.referenceDensity));
        if(settings.trace)std::cout<<"WATER_PRESSURE iteration="<<iteration<<" normalized="<<maximum<<" row="<<largest<<" value="<<current.rows[largest].Value()<<" tol="<<tolerance[largest]<<" gasV="<<(largest>=nw?gas.volume[largest-nw].Value():0.)<<" pressure="<<x[largest].Value()<<" density="<<(largest>=nw?current.density[largest-nw].Value():0.)<<std::endl;
        if(maximum<=1.){WaterGasProjectionResult next;next.headDatum=datum;next.head.assign(x.begin(),x.begin()+nw);next.gasAbsolutePressure.assign(x.begin()+nw,x.end());for(auto p:next.gasAbsolutePressure)next.gasPressure.push_back(p-Real(settings.referencePressure));next.gasMass=current.mass;next.displacedGasVolume=current.volume;next.liquidFlux=current.q;next.liquidHeadDifference=current.difference;next.liquidReferencePressure=current.referencePressure;
            for(std::size_t g=0;g<ng;++g){next.gasVolume.push_back(current.mass[g]/current.density[g]);double strict=std::min(settings.massTolerance/settings.referenceDensity,settings.relativeTolerance*gas.volume[g].Value());next.strictMinorityResidual=std::max(next.strictMinorityResidual,std::abs(current.rows[nw+g].Value())/strict);}
            for(std::size_t g=0;g<ng;++g)if(!(current.mass[g].Value()>0.&&next.gasVolume[g].Value()>0.))return Fail(error,"gas capacity or mass exhausted by liquid displacement");
            for(auto l:gas.links)next.gasLinkMass.push_back(Real(settings.dt*l.massConductance)*(x[nw+l.left]-x[nw+l.right]));
            for(auto v:gas.vents){Real mass=Real(settings.dt*v.massConductance)*(x[nw+v.node]-Real(settings.referencePressure)-Real(v.gaugePressure));next.gasVentMass.push_back(mass);next.externalGasMass=next.externalGasMass+mass;}
            Real mass=next.externalGasMass;for(std::size_t g=0;g<ng;++g)mass=mass+next.gasMass[g]-gas.mass[g];
            if(std::abs(mass.Value())>settings.massTolerance)return Fail(error,"liquid/gas external mass ledger failed");
            next.maximumNormalizedResidual=maximum;next.iterations=iteration+1;next.linearApplications=applications;next.linearFallbacks=fallbacks;next.elapsedMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();output=std::move(next);error.clear();return true;
        }
        // Rebuild only the accelerator's finite-air storage at the current
        // EOS state. Near-vacuum/compressed pockets have very different
        // compliance from their initial ambient value.
        std::fill(storage.begin(),storage.end(),0.);edges.resize(liquidEdges);

        for(std::size_t g=0;g<ng;++g){
            double compliance=(current.mass[g]*Real(settings.referenceDensity/settings.referencePressure)/(current.density[g]*current.density[g])).Value();
            if(!std::isfinite(compliance))return Fail(error,"gas iterate has nonfinite pressure compliance");
            // Search directions may pass outside the admissible mass domain.
            // Only the accelerator uses initial positive storage there; the
            // exact Jacobian and final positive mass/volume gates are unchanged.
            if(compliance<0.)compliance=gas.volume[g].Value()/settings.referencePressure;
            storage[nw+g]=compliance;

        }
        for(auto l:gas.links){double conductance=settings.dt*l.massConductance*.5*(1./current.density[l.left].Value()+1./current.density[l.right].Value());edges.push_back({nw+l.left,nw+l.right,conductance});
}
        for(auto v:gas.vents){storage[nw+v.node]+=settings.dt*v.massConductance/current.density[v.node].Value();}
        std::vector<std::map<std::size_t,Real>> matrix(n);
        for(const auto& r:water.regions){auto m=r.response.free.size();
            for(std::size_t a=0;a<m;++a)for(std::size_t b=0;b<m;++b){auto oa=r.owners[r.response.free[a]],ob=r.owners[r.response.free[b]];auto i=oa.shared!=SIZE_MAX?oa.shared:nw+oa.gas,j=ob.shared!=SIZE_MAX?ob.shared:nw+ob.gas;matrix[i][j]=matrix[i][j]+Real(settings.dt*settings.dt*r.response.mobility[a*m+b]);}}
        for(std::size_t g=0;g<ng;++g)matrix[nw+g][nw+g]=matrix[nw+g][nw+g]+current.mass[g]*Real(settings.referenceDensity/settings.referencePressure)/(current.density[g]*current.density[g]);
        for(auto l:gas.links){auto a=nw+l.left,b=nw+l.right;Real ka=Real(settings.dt*l.massConductance)/current.density[l.left],kb=Real(settings.dt*l.massConductance)/current.density[l.right];matrix[a][a]=matrix[a][a]+ka;matrix[a][b]=matrix[a][b]-ka;matrix[b][b]=matrix[b][b]+kb;matrix[b][a]=matrix[b][a]-kb;}
        for(auto v:gas.vents)matrix[nw+v.node][nw+v.node]=matrix[nw+v.node][nw+v.node]+Real(settings.dt*v.massConductance)/current.density[v.node];
        WaterPressureILU ilu;bool fast=ilu.Build(matrix,error);
        if(!fast){++fallbacks;std::vector<bool> condense(n,true);if(!factor.Build(storage,edges,condense,error))return false;}
        auto jacobian=[&](const std::vector<double>& direction,std::vector<double>& applied){std::vector<Real> rows(n),dm(ng);
            for(const auto& r:water.regions){auto m=r.response.free.size();std::vector<double> h(m);for(std::size_t a=0;a<m;++a){auto o=r.owners[r.response.free[a]];h[a]=direction[o.shared!=SIZE_MAX?o.shared:nw+o.gas];}
                std::vector<Real> delta(m);
                for(std::size_t a=0;a<m;++a)if(a!=r.response.reference){for(std::size_t b=0;b<m;++b)if(b!=r.response.reference)delta[a]=delta[a]-Real(settings.dt*r.response.mobility[a*m+b])*Real(h[b]-h[r.response.reference]);delta[r.response.reference]=delta[r.response.reference]-delta[a];}
                for(std::size_t a=0;a<m;++a){Real dq=delta[a];auto o=r.owners[r.response.free[a]];auto i=o.shared!=SIZE_MAX?o.shared:nw+o.gas;rows[i]=rows[i]-Real(settings.dt)*dq;}}
            for(auto l:gas.links){Real amount=Real(settings.dt*l.massConductance)*Real(direction[nw+l.left]-direction[nw+l.right]);dm[l.left]=dm[l.left]-amount;dm[l.right]=dm[l.right]+amount;}
            for(auto v:gas.vents)dm[v.node]=dm[v.node]-Real(settings.dt*v.massConductance)*Real(direction[nw+v.node]);
            for(std::size_t g=0;g<ng;++g)rows[nw+g]=rows[nw+g]-dm[g]/current.density[g]+current.mass[g]*Real(settings.referenceDensity/settings.referencePressure)*Real(direction[nw+g])/(current.density[g]*current.density[g]);
            applied=normalize(rows);return true;};
        // Diagonal scaling is explicit and affects only the iterative solve.
        auto precondition=[&](const std::vector<double>& r,std::vector<double>& v){auto physical=r;for(std::size_t i=0;i<n;++i)physical[i]*=diagonal[i];return fast?ilu.Apply(physical,v,error):factor.Apply(physical,v,error);};
        auto rhs=normalize(current.rows);double norm=Norm(rhs);for(auto& v:rhs)v=-v;CutKrylovStats stats;std::vector<Real> correction;
        bool linearSolved=SolveCutCoupledKrylov(rhs,jacobian,precondition,correction,stats,error,50,12,.005);
        applications+=stats.applications;
        if(!linearSolved){
            if(!fast)return false;
            fast=false;++fallbacks;std::vector<bool> condense(n,true);
            if(!factor.Build(storage,edges,condense,error))return false;
            stats={};linearSolved=SolveCutCoupledKrylov(rhs,jacobian,precondition,correction,stats,error,50,12,.005);
            applications+=stats.applications;if(!linearSolved)return false;
        }
        double line=1.;for(std::size_t g=0;g<ng;++g)if(correction[nw+g].Value()<0.)line=std::min(line,.9*x[nw+g].Value()/-correction[nw+g].Value());
        bool accepted=false;for(int trial=0;trial<30;++trial){auto candidate=x;for(std::size_t i=0;i<n;++i)candidate[i]=candidate[i]+correction[i]*Real(line);Evaluation sample;
            if(evaluate(candidate,sample)&&Norm(normalize(sample.rows))<norm*(1.-1e-4*line)){x=std::move(candidate);current=std::move(sample);accepted=true;break;}line*=.5;}
        if(!accepted)return Fail(error,"liquid/gas pressure line search exhausted");
    }
    return Fail(error,"liquid/gas pressure iteration budget exhausted");
}
bool ProjectWaterGasPressure(const WaterMimeticGeometry& water,const WaterGasGeometry& gas,
    const WaterGasSettings& settings,WaterGasProjectionResult& output,std::string& error,int limit){
    return ProjectWaterGasPressure(water,gas,settings,output,error,nullptr,limit);
}

}
