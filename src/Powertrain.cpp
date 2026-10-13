#include "Powertrain.h"
#include "SaveArchive.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace {
constexpr double Pi=3.14159265358979323846;
bool Finite(double x){return std::isfinite(x);}
bool Axis(glm::vec3 a){return Finite(a.x)&&Finite(a.y)&&Finite(a.z)&&glm::dot(a,a)>1e-12f&&Finite(glm::dot(a,a));}
bool Name(const std::string& s){return !s.empty()&&s.size()<=64&&s.find('\0')==std::string::npos;}
// Complete-pivot elimination also solves consistent rank-deficient systems.
// A redundant clutch/brake at standstill need not create an arbitrary spring
// or diagonal regularizer: its indistinguishable impulse is chosen as zero.
bool LinearSolve(std::vector<double> a,std::vector<double> b,std::vector<double>& answer,unsigned n){
    answer.assign(n,0);if(!n)return true;
    double matrixScale=0,rhsScale=0;for(double v:a)matrixScale=std::max(matrixScale,std::abs(v));
    for(double v:b)rhsScale=std::max(rhsScale,std::abs(v));
    std::vector<unsigned> columns(n);for(unsigned i=0;i<n;++i)columns[i]=i;
    unsigned rank=0;
    for(unsigned k=0;k<n;++k){
        unsigned row=k,column=k;double pivot=0;
        for(unsigned i=k;i<n;++i)for(unsigned j=k;j<n;++j)if(std::abs(a[i*n+j])>pivot){pivot=std::abs(a[i*n+j]);row=i;column=j;}
        if(pivot<=matrixScale*1e-12)break;
        if(row!=k){for(unsigned j=0;j<n;++j)std::swap(a[k*n+j],a[row*n+j]);std::swap(b[k],b[row]);}
        if(column!=k){for(unsigned i=0;i<n;++i)std::swap(a[i*n+k],a[i*n+column]);std::swap(columns[k],columns[column]);}
        for(unsigned i=k+1;i<n;++i){const double factor=a[i*n+k]/a[k*n+k];a[i*n+k]=0;
            for(unsigned j=k+1;j<n;++j)a[i*n+j]-=factor*a[k*n+j];
            b[i]-=factor*b[k];}
        ++rank;
    }
    for(unsigned i=rank;i<n;++i)if(std::abs(b[i])>1e-9*std::max(1.0,rhsScale))return false;
    std::vector<double> x(n,0);
    for(unsigned k=rank;k>0;--k){const unsigned i=k-1;double value=b[i];for(unsigned j=i+1;j<rank;++j)value-=a[i*n+j]*x[j];x[i]=value/a[i*n+i];}
    for(unsigned i=0;i<n;++i){answer[columns[i]]=x[i];if(!Finite(x[i]))return false;}return true;
}
bool PositiveDefinite(const std::vector<double>& w,const std::vector<unsigned>& active,unsigned n){
    const unsigned m=unsigned(active.size());std::vector<double> l(m*m,0);
    for(unsigned i=0;i<m;++i)for(unsigned j=0;j<=i;++j){
        double v=w[active[i]*n+active[j]];
        if(std::abs(v-w[active[j]*n+active[i]])>1e-9*std::max({1.0,std::abs(v),std::abs(w[active[j]*n+active[i]])}))return false;
        for(unsigned k=0;k<j;++k)v-=l[i*m+k]*l[j*m+k];
        if(i==j){if(!(v>std::max(1e-14,w[active[i]*n+active[i]]*1e-12)))return false;l[i*m+j]=std::sqrt(v);}
        else l[i*m+j]=v/l[j*m+j];
    }
    return true;
}
struct Row {
    std::vector<double> j;
    double scale=1,limit=std::numeric_limits<double>::infinity();
    int link=-1,brake=-1;
};
// Convex, bounded impulse solve. Unbounded gear rows are solved exactly as
// part of each active-set system. A clutch/brake at its cap deliberately
// retains physical slip; no iterative gear drift or fake speed target exists.
bool BoundedSolve(const std::vector<double>& a,const std::vector<double>& b,
    const std::vector<Row>& rows,std::vector<double>& lambda,unsigned& iterations){
    const unsigned m=unsigned(rows.size());lambda.assign(m,0);iterations=0;
    std::vector<int> status(m,0);double scale=1;for(double value:b)scale=std::max(scale,std::abs(value));
    const double tolerance=1e-10*scale;
    // A finite safety boundary prevents an adversarial degenerate graph from
    // consuming unbounded runtime. Failure returns no partial physical impulse.
    const unsigned budget=std::max(32u,8*m*m+8);
    for(unsigned iteration=0;iteration<budget;++iteration){
        iterations=iteration+1;std::vector<double> gradient=b;
        for(unsigned i=0;i<m;++i)for(unsigned j=0;j<m;++j)gradient[i]+=a[i*m+j]*lambda[j];
        std::vector<unsigned> free;for(unsigned i=0;i<m;++i)if(!status[i])free.push_back(i);
        const unsigned f=unsigned(free.size());std::vector<double> sub(f*f),rhs(f),p;
        for(unsigned i=0;i<f;++i){rhs[i]=-gradient[free[i]];for(unsigned j=0;j<f;++j)sub[i*f+j]=a[free[i]*m+free[j]];}
        if(!LinearSolve(std::move(sub),std::move(rhs),p,f))return false;
        double step=1;int blocking=-1,side=0;double magnitude=0;
        for(unsigned i=0;i<f;++i){const unsigned index=free[i];magnitude=std::max(magnitude,std::abs(p[i]));
            if(!Finite(rows[index].limit)||std::abs(p[i])<tolerance)continue;
            const int candidateSide=p[i]>0?1:-1;
            const double candidate=(candidateSide*rows[index].limit-lambda[index])/p[i];
            if(candidate<step-1e-12||(candidate<=step+1e-12&&blocking<0&&candidate<1)){step=std::max(0.0,candidate);blocking=int(index);side=candidateSide;}
        }
        for(unsigned i=0;i<f;++i)lambda[free[i]]+=step*p[i];
        if(blocking>=0&&step<1-1e-12){lambda[unsigned(blocking)]=side*rows[unsigned(blocking)].limit;status[unsigned(blocking)]=side;continue;}
        // Recompute after the exact free minimization, then release the most
        // strongly violated bound. Stable index order resolves equal choices.
        gradient=b;for(unsigned i=0;i<m;++i)for(unsigned j=0;j<m;++j)gradient[i]+=a[i*m+j]*lambda[j];
        int release=-1;double violation=tolerance;
        for(unsigned i=0;i<m;++i)if(status[i]){const double value=status[i]*gradient[i];if(value>violation){violation=value;release=int(i);}}
        if(release>=0){status[unsigned(release)]=0;continue;}
        for(unsigned i=0;i<m;++i)if(!status[i]&&std::abs(gradient[i])>tolerance*10)return false;
        (void)magnitude;return true;
    }
    return false;
}
double Curve(const std::vector<PowertrainTorquePoint>& points,double rpm){
    if(points.empty()||rpm<points.front().rpm||rpm>points.back().rpm)return 0;
    const auto upper=std::upper_bound(points.begin(),points.end(),rpm,[](double value,const auto& point){return value<point.rpm;});
    if(upper==points.end())return points.back().torque;
    if(upper==points.begin())return points.front().torque;
    const auto& a=*(upper-1);const auto& b=*upper;return a.torque+(b.torque-a.torque)*(rpm-a.rpm)/(b.rpm-a.rpm);
}
}

bool ValidatePowertrainSettings(const PowertrainSettings& s,std::string& error){
    const unsigned n=unsigned(s.shafts.size());
    if(!n||n>kPowertrainMaxShafts||s.sources.size()>kPowertrainMaxSources||s.brakes.size()>kPowertrainMaxBrakes||s.links.size()+s.brakes.size()>kPowertrainMaxRows){error="powertrain requires 1..32 shafts, at most 32 sources/brakes and 64 total constraints";return false;}
    std::set<std::string> shaftNames,sourceNames,linkNames,brakeNames;
    std::set<std::pair<int,uint64_t>> targets;
    for(const auto& shaft:s.shafts){
        if(!Name(shaft.name)||!shaftNames.insert(shaft.name).second||int(shaft.kind)<0||int(shaft.kind)>2||
            !Finite(shaft.inertia)||shaft.inertia<=1e-9||!Finite(shaft.initialAngularVelocity)||!Finite(shaft.viscousDrag)||shaft.viscousDrag<0||!Axis(shaft.axis)||
            (shaft.kind!=PowertrainShaftKind::Internal&&(!shaft.target||shaft.target==shaft.bearing||!targets.emplace(int(shaft.kind),shaft.target).second))||
            (shaft.kind==PowertrainShaftKind::Internal&&shaft.target)||(shaft.kind==PowertrainShaftKind::Wheel&&shaft.bearing)){
            error="invalid or duplicate powertrain shaft name, inertia, axis, bearing or terminal";return false;
        }
    }
    for(const auto& source:s.sources){
        if(!Name(source.name)||!sourceNames.insert(source.name).second||source.shaft>=n||int(source.kind)<0||int(source.kind)>2||
            !Finite(source.maxTorque)||source.maxTorque<0||!Finite(source.maxPower)||source.maxPower<0||!Finite(source.starterTorque)||source.starterTorque<0||
            source.torqueCurve.size()>64||(source.kind==PowertrainSourceKind::Combustion&&source.torqueCurve.size()<2)||
            (source.kind==PowertrainSourceKind::Electric&&source.maxPower<=0)){
            error="invalid powertrain source, torque, power or curve";return false;
        }
        double last=-1;for(const auto& point:source.torqueCurve){if(!Finite(point.rpm)||point.rpm<0||point.rpm<=last||!Finite(point.torque)||point.torque<0){error="powertrain torque curve requires increasing nonnegative RPM and torque";return false;}last=point.rpm;}
    }
    for(const auto& link:s.links){
        if(!Name(link.name)||!linkNames.insert(link.name).second||int(link.kind)<0||int(link.kind)>2||link.input>=n||link.output>=n||link.input==link.output||
            (link.kind==PowertrainLinkKind::Differential&&(link.outputB>=n||link.outputB==link.input||link.outputB==link.output))||
            !Finite(link.ratio)||std::abs(link.ratio)<1e-6||std::abs(link.ratio)>1e6||!Finite(link.maxTorque)||link.maxTorque<0){error="invalid powertrain gear, clutch or differential";return false;}
    }
    for(const auto& brake:s.brakes)if(!Name(brake.name)||!brakeNames.insert(brake.name).second||brake.shaft>=n||!Finite(brake.maxTorque)||brake.maxTorque<0){error="invalid powertrain brake";return false;}
    return true;
}
bool ValidatePowertrainCommand(const PowertrainSettings& s,const PowertrainCommand& c,std::string& error){
    if(c.sources.size()!=s.sources.size()||c.links.size()!=s.links.size()||c.brakes.size()!=s.brakes.size()){error="powertrain command shape differs from authored assembly";return false;}
    for(unsigned i=0;i<c.sources.size();++i){const auto& value=c.sources[i];
        if(!Finite(value.input)||value.input< -1||value.input>1||!Finite(value.customTorque)||std::abs(value.customTorque)>s.sources[i].maxTorque||
            !Finite(value.starter)||value.starter<0||value.starter>1||(s.sources[i].kind==PowertrainSourceKind::Combustion&&value.input<0)){
            error="invalid powertrain source control or custom torque capacity";return false;
        }
    }
    for(const auto& value:c.links)if(!Finite(value.engagement)||value.engagement<0||value.engagement>1||!Finite(value.ratio)||
        (value.overrideRatio&&(std::abs(value.ratio)<1e-6||std::abs(value.ratio)>1e6))){error="invalid powertrain link control or ratio";return false;}
    for(const auto& value:c.brakes)if(!Finite(value.engagement)||value.engagement<0||value.engagement>1){error="invalid powertrain brake engagement";return false;}
    return true;
}
bool PowertrainSettingsEqual(const PowertrainSettings& a,const PowertrainSettings& b){
    if(a.enabled!=b.enabled||a.shafts.size()!=b.shafts.size()||a.sources.size()!=b.sources.size()||a.links.size()!=b.links.size()||a.brakes.size()!=b.brakes.size())return false;
    for(unsigned i=0;i<a.shafts.size();++i){const auto& x=a.shafts[i];const auto& y=b.shafts[i];if(x.name!=y.name||x.kind!=y.kind||x.inertia!=y.inertia||x.initialAngularVelocity!=y.initialAngularVelocity||x.viscousDrag!=y.viscousDrag||x.target!=y.target||x.bearing!=y.bearing||x.axis!=y.axis)return false;}
    for(unsigned i=0;i<a.sources.size();++i){const auto& x=a.sources[i];const auto& y=b.sources[i];if(x.name!=y.name||x.shaft!=y.shaft||x.kind!=y.kind||x.maxTorque!=y.maxTorque||x.maxPower!=y.maxPower||x.starterTorque!=y.starterTorque||x.torqueCurve.size()!=y.torqueCurve.size())return false;for(unsigned j=0;j<x.torqueCurve.size();++j)if(x.torqueCurve[j].rpm!=y.torqueCurve[j].rpm||x.torqueCurve[j].torque!=y.torqueCurve[j].torque)return false;}
    for(unsigned i=0;i<a.links.size();++i){const auto& x=a.links[i];const auto& y=b.links[i];if(x.name!=y.name||x.kind!=y.kind||x.input!=y.input||x.output!=y.output||x.outputB!=y.outputB||x.ratio!=y.ratio||x.maxTorque!=y.maxTorque)return false;}
    for(unsigned i=0;i<a.brakes.size();++i){const auto& x=a.brakes[i];const auto& y=b.brakes[i];if(x.name!=y.name||x.shaft!=y.shaft||x.maxTorque!=y.maxTorque)return false;}return true;
}

void PowertrainInstance::Reset(const PowertrainSettings& value){
    settings=value;command={};state={};std::string error;
    if(!ValidatePowertrainSettings(settings,error)){state.converged=false;state.diagnostic=error;absoluteSpin.clear();internalInitialized.clear();return;}
    const auto n=settings.shafts.size();
    state.shafts.resize(n);absoluteSpin.resize(n);internalInitialized.assign(n,false);
    for(unsigned i=0;i<n;++i){state.shafts[i].angularVelocity=settings.shafts[i].initialAngularVelocity;absoluteSpin[i]=settings.shafts[i].initialAngularVelocity;}
    command.sources.resize(settings.sources.size());command.links.resize(settings.links.size());command.brakes.resize(settings.brakes.size());
    state.sourceTorque.resize(settings.sources.size());state.links.resize(settings.links.size());state.brakeTorque.resize(settings.brakes.size());
    linkBudgetUsed.assign(settings.links.size(),0);brakeBudgetUsed.assign(settings.brakes.size(),0);
}
std::vector<double> PowertrainInstance::Step(double dt,const std::vector<double>& velocity,
    const std::vector<double>& w,const std::vector<bool>& available,bool integrateSources){
    const unsigned n=unsigned(settings.shafts.size());std::vector<double> zero(n,0);
    if(!integrateSources&&!state.converged)return zero;
    const auto before=state;const auto beforeLinkBudget=linkBudgetUsed,beforeBrakeBudget=brakeBudgetUsed;
    state.available=false;state.converged=true;state.diagnostic.clear();state.residual=0;
    if(integrateSources){state.iterations=0;state.constraintEnergyChange=0;
        state.sourceTorque.assign(settings.sources.size(),0);state.brakeTorque.assign(settings.brakes.size(),0);state.links.assign(settings.links.size(),{});
        linkBudgetUsed.assign(settings.links.size(),0);brakeBudgetUsed.assign(settings.brakes.size(),0);}
    std::string error;
    auto fail=[&](const std::string& message){
        if(!integrateSources){state=before;linkBudgetUsed=beforeLinkBudget;brakeBudgetUsed=beforeBrakeBudget;}
        else {state.sourceTorque.assign(settings.sources.size(),0);state.brakeTorque.assign(settings.brakes.size(),0);state.links.assign(settings.links.size(),{});state.kineticEnergy=0;state.constraintEnergyChange=0;for(auto& shaft:state.shafts)shaft.torque=0;linkBudgetUsed.assign(settings.links.size(),0);brakeBudgetUsed.assign(settings.brakes.size(),0);}
        for(unsigned i=0;i<state.shafts.size()&&i<velocity.size();++i)state.shafts[i].angularVelocity=Finite(velocity[i])?velocity[i]:before.shafts[i].angularVelocity;
        state.available=false;state.converged=false;state.diagnostic=message;return zero;
    };
    if(!ValidatePowertrainSettings(settings,error)||!ValidatePowertrainCommand(settings,command,error))return fail(error);
    if(!Finite(dt)||dt<=0||velocity.size()!=n||available.size()!=n||w.size()!=n*n||state.shafts.size()!=n)return fail("invalid powertrain step shape or interval");
    std::vector<unsigned> active;
    for(unsigned i=0;i<n;++i){state.shafts[i].available=available[i];if(integrateSources)state.shafts[i].torque=0;state.shafts[i].angularVelocity=velocity[i];if(!Finite(velocity[i]))return fail("nonfinite powertrain velocity");if(available[i])active.push_back(i);}
    for(double value:w)if(!Finite(value))return fail("nonfinite powertrain inverse mass");
    for(unsigned i=0;i<n;++i)if(!available[i])for(unsigned j=0;j<n;++j)if(w[i*n+j]!=0||w[j*n+i]!=0)return fail("unavailable shaft has a nonzero inverse mass row");
    if(!PositiveDefinite(w,active,n))return fail("powertrain inverse mass must be symmetric positive definite on available shafts");
    state.available=!active.empty();if(!settings.enabled||active.empty())return zero;
    std::vector<double> sourceImpulse(n,0),omega=velocity;
    if(integrateSources){
        for(unsigned i=0;i<settings.sources.size();++i){const auto& source=settings.sources[i];const auto& control=command.sources[i];if(!available[source.shaft])continue;
            double torque=0;
            if(source.kind==PowertrainSourceKind::Combustion)torque=control.input*std::min(source.maxTorque,Curve(source.torqueCurve,velocity[source.shaft]*60/(2*Pi)));
            else if(source.kind==PowertrainSourceKind::Electric){const double speed=std::abs(velocity[source.shaft]);torque=control.input*std::min(source.maxTorque,speed>1e-12?source.maxPower/speed:source.maxTorque);}
            else torque=control.customTorque;
            torque+=control.starter*source.starterTorque;state.sourceTorque[i]=torque;sourceImpulse[source.shaft]+=torque*dt;
        }
        // Bearing drag is passive and implicit: (I + dt W D) omega_free
        // = omega + W sourceImpulse. It cannot reverse a shaft through an
        // explicit overshoot when a light rotor carries large drag.
        const unsigned m=unsigned(active.size());std::vector<double> a(m*m),rhs(m),resolved;
        for(unsigned i=0;i<m;++i){const auto index=active[i];rhs[i]=velocity[index];
            for(unsigned j=0;j<n;++j)rhs[i]+=w[index*n+j]*sourceImpulse[j];
            for(unsigned j=0;j<m;++j)a[i*m+j]=(i==j?1:0)+dt*w[index*n+active[j]]*settings.shafts[active[j]].viscousDrag;}
        if(!LinearSolve(std::move(a),std::move(rhs),resolved,m))return fail("powertrain drag solve failed");
        for(unsigned i=0;i<m;++i){const auto index=active[i];omega[index]=resolved[i];sourceImpulse[index]-=dt*settings.shafts[index].viscousDrag*resolved[i];}
    }
    std::vector<Row> rows;
    auto add=[&](Row row){double inverse=0;for(unsigned i=0;i<n;++i)for(unsigned j=0;j<n;++j)inverse+=row.j[i]*w[i*n+j]*row.j[j];
        if(!(inverse>0)||!Finite(inverse))return false;
        row.scale=std::sqrt(inverse);for(auto& value:row.j)value/=row.scale;if(Finite(row.limit))row.limit*=row.scale;rows.push_back(std::move(row));return true;};
    for(unsigned i=0;i<settings.links.size();++i){const auto& link=settings.links[i];const auto& control=command.links[i];
        if(!control.enabled||!available[link.input]||!available[link.output]||(link.kind==PowertrainLinkKind::Differential&&!available[link.outputB]))continue;
        if(link.kind==PowertrainLinkKind::Clutch&&(control.engagement==0||link.maxTorque==0))continue;
        Row row;row.j.assign(n,0);row.link=int(i);const double ratio=control.overrideRatio?control.ratio:link.ratio;
        if(link.kind==PowertrainLinkKind::Differential){row.j[link.input]=-2*ratio;row.j[link.output]=row.j[link.outputB]=1;}
        else {row.j[link.input]=-ratio;row.j[link.output]=1;}
        if(link.kind==PowertrainLinkKind::Clutch){row.limit=std::max(0.0,link.maxTorque*control.engagement*dt-linkBudgetUsed[i]);if(row.limit==0)continue;}
        if(!add(std::move(row)))return fail("powertrain link has no independent rotational response");
    }
    for(unsigned i=0;i<settings.brakes.size();++i){const auto& brake=settings.brakes[i];const double limit=std::max(0.0,brake.maxTorque*command.brakes[i].engagement*dt-brakeBudgetUsed[i]);
        if(limit==0||!available[brake.shaft])continue;
        Row row;row.j.assign(n,0);row.j[brake.shaft]=1;row.limit=limit;row.brake=int(i);if(!add(std::move(row)))return fail("powertrain brake has no rotational response");}
    const unsigned m=unsigned(rows.size());std::vector<double> a(m*m,0),b(m,0),lambda;
    std::vector<double> jw(m*n,0);
    for(unsigned i=0;i<m;++i)for(unsigned k=0;k<n;++k)if(rows[i].j[k]!=0){b[i]+=rows[i].j[k]*omega[k];for(unsigned j=0;j<n;++j)jw[i*n+j]+=rows[i].j[k]*w[k*n+j];}
    for(unsigned i=0;i<m;++i)for(unsigned j=0;j<m;++j)for(unsigned k=0;k<n;++k)a[i*m+j]+=jw[i*n+k]*rows[j].j[k];
    unsigned iterations=0;
    if(!BoundedSolve(a,b,rows,lambda,iterations))return fail("powertrain bounded constraint solve did not converge");
    state.iterations+=iterations;
    std::vector<double> impulse=sourceImpulse;
    for(unsigned i=0;i<m;++i){for(unsigned j=0;j<n;++j)impulse[j]+=rows[i].j[j]*lambda[i];
        state.constraintEnergyChange+=lambda[i]*b[i];for(unsigned j=0;j<m;++j)state.constraintEnergyChange+=.5*lambda[i]*a[i*m+j]*lambda[j];}
    for(unsigned i=0;i<n;++i){double value=velocity[i];for(unsigned j=0;j<n;++j)value+=w[i*n+j]*impulse[j];if(!Finite(value))return fail("nonfinite resolved powertrain velocity");state.shafts[i].angularVelocity=value;state.shafts[i].torque+=impulse[i]/dt;}
    for(unsigned i=0;i<m;++i){const auto& row=rows[i];double slip=0;for(unsigned j=0;j<n;++j)slip+=row.j[j]*state.shafts[j].angularVelocity;
        if(!Finite(row.limit))state.residual=std::max(state.residual,std::abs(slip*row.scale));
        const double physicalImpulse=lambda[i]/row.scale;
        (void)slip;
        if(row.link>=0){auto& result=state.links[unsigned(row.link)];result.impulse+=physicalImpulse;result.torque+=physicalImpulse/dt;linkBudgetUsed[unsigned(row.link)]+=std::abs(physicalImpulse);}
        if(row.brake>=0){state.brakeTorque[unsigned(row.brake)]+=physicalImpulse/dt;brakeBudgetUsed[unsigned(row.brake)]+=std::abs(physicalImpulse);}
    }
    // Even a capacity-exhausted clutch remains an active, slipping coupling;
    // its telemetry must not imply that a skipped zero-cap row is locked.
    for(unsigned i=0;i<settings.links.size();++i){const auto& link=settings.links[i];const auto& control=command.links[i];auto& result=state.links[i];
        result.active=control.enabled&&available[link.input]&&available[link.output]&&(link.kind!=PowertrainLinkKind::Differential||available[link.outputB])&&(link.kind!=PowertrainLinkKind::Clutch||control.engagement>0);
        if(!result.active){result.slip=0;result.saturated=false;continue;}
        const double ratio=control.overrideRatio?control.ratio:link.ratio;
        result.slip=state.shafts[link.output].angularVelocity-ratio*state.shafts[link.input].angularVelocity;
        if(link.kind==PowertrainLinkKind::Differential)result.slip+=state.shafts[link.outputB].angularVelocity-ratio*state.shafts[link.input].angularVelocity;
        const double capacity=link.maxTorque*control.engagement*dt;result.saturated=link.kind==PowertrainLinkKind::Clutch&&linkBudgetUsed[i]>=capacity-1e-9*std::max(1.0,capacity);
    }
    const unsigned count=unsigned(active.size());std::vector<double> metric(count*count),speed(count),momentum;
    for(unsigned i=0;i<count;++i){speed[i]=state.shafts[active[i]].angularVelocity;for(unsigned j=0;j<count;++j)metric[i*count+j]=w[active[i]*n+active[j]];}
    state.kineticEnergy=0;if(LinearSolve(std::move(metric),speed,momentum,count))for(unsigned i=0;i<count;++i)state.kineticEnergy+=.5*speed[i]*momentum[i];
    if(!Finite(state.kineticEnergy)||!Finite(state.constraintEnergyChange))return fail("nonfinite powertrain energy metric");
    return impulse;
}
void PowertrainInstance::AdvanceAngles(double dt){
    if(!Finite(dt)||dt<=0||!settings.enabled)return;
    for(auto& shaft:state.shafts)if(shaft.available)shaft.angle=std::remainder(shaft.angle+shaft.angularVelocity*dt,2*Pi);
}
void PowertrainInstance::Persist(SaveArchive& archive){
    uint32_t shafts=uint32_t(settings.shafts.size()),sources=uint32_t(settings.sources.size()),links=uint32_t(settings.links.size()),brakes=uint32_t(settings.brakes.size());
    archive(shafts,sources,links,brakes);
    archive.Require(shafts==settings.shafts.size()&&sources==settings.sources.size()&&links==settings.links.size()&&brakes==settings.brakes.size(),"saved powertrain shape mismatch");
    archive.Require(state.shafts.size()==shafts&&absoluteSpin.size()==shafts&&internalInitialized.size()==shafts,"invalid powertrain instance shape");
    // Relative speed is an observation of the current physical endpoint and
    // bearing. It can lag absoluteSpin/body/wheel state until publication, so
    // it must not be a second durable authority for that same momentum.
    for(unsigned i=0;i<shafts;++i){archive(state.shafts[i].angle,absoluteSpin[i]);bool initialized=internalInitialized[i];archive(initialized);internalInitialized[i]=initialized;}
    for(auto& value:command.sources)archive(value.input,value.customTorque,value.starter);
    for(auto& value:command.links)archive(value.enabled,value.overrideRatio,value.engagement,value.ratio);
    for(auto& value:command.brakes)archive(value.engagement);
    std::string error;const bool valid=ValidatePowertrainCommand(settings,command,error);archive.Require(valid,error);
    if(archive.reading){state.available=false;state.converged=true;state.diagnostic.clear();state.iterations=0;state.residual=0;state.kineticEnergy=0;state.constraintEnergyChange=0;
        state.sourceTorque.assign(sources,0);state.links.assign(links,{});state.brakeTorque.assign(brakes,0);linkBudgetUsed.assign(links,0);brakeBudgetUsed.assign(brakes,0);for(auto& shaft:state.shafts){shaft.available=false;shaft.angularVelocity=0;shaft.torque=0;}}
}
