#include "FluidHydrostatics.h"
#include "GravityField.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0,failures=0;
void Check(bool ok,const char* name) { ++checks; if (!ok) { ++failures; std::cout<<"FAIL "<<name<<'\n'; } }
void Near(double value,double expected,double tol,const char* name) { Check(std::abs(value-expected)<=tol,name); if(std::abs(value-expected)>tol)std::cout<<" observed="<<value<<" expected="<<expected<<" tolerance="<<tol<<'\n'; }
void VectorNear(glm::vec3 value,glm::vec3 expected,float tol,const char* name) { Check(glm::length(value-expected)<=tol,name); }
struct Uniform : GravityField { glm::vec3 value; explicit Uniform(glm::vec3 g):value(g) {} glm::vec3 Sample(const glm::vec3&) const override{return value;} };
std::vector<FluidParticle> Pool(float spacing,glm::vec3 velocity=glm::vec3(0),glm::vec3 acceleration=glm::vec3(0),bool hole=false) {
    std::vector<FluidParticle> out;
    const int width=static_cast<int>(std::lround(4/spacing)), depth=static_cast<int>(std::lround(3/spacing));
    for(int z=0;z<width;++z)for(int y=0;y<depth;++y)for(int x=0;x<width;++x) {
        const glm::vec3 p(-2+(x+0.5f)*spacing,-(y+0.5f)*spacing,-2+(z+0.5f)*spacing);
        if(hole && std::abs(p.x)<0.5f && std::abs(p.z)<0.5f && p.y> -1.5f)continue;
        out.push_back({p,p,velocity,1000*spacing*spacing*spacing,acceleration});
    }
    return out;
}
void Rotate(std::vector<FluidParticle>& p,const glm::quat& q,const glm::vec3& t) {
    for(auto& x:p){x.position=q*x.position+t;x.previousPosition=x.position;x.velocity=q*x.velocity;x.acceleration=q*x.acceleration;}
}
}
int main() {
    const auto begin=std::chrono::steady_clock::now();
    std::cout<<std::setprecision(12);
    const Uniform gravity({0,-9.81f,0}), zero({0,0,0});
    const auto box=MakeBoxFluidVolume({.5f,.5f,.5f});
    const auto sphere=MakeSphereFluidVolume(.5f);
    const auto compound=MakeCompoundFluidVolume({{{-.25f,0,0},{.5f,.5f,.5f}},{{.25f,0,0},{.5f,.5f,.5f}}});
    Near(box.totalVolume,1,1e-7,"box analytic volume");
    Near(sphere.totalVolume,3.14159265358979323846/6,5e-8,"sphere analytic volume");
    Near(compound.totalVolume,1.5,1e-7,"overlapping compound union volume");
    for(const auto* q:{&box,&sphere,&compound}){double sum=0;glm::dvec3 moment(0);for(const auto&s:q->samples){sum+=s.volume;moment+=static_cast<double>(s.volume)*glm::dvec3(s.localPosition);}Near(sum,q->totalVolume,2e-7,"quadrature volume sum");Check(glm::length(moment)<2e-7,"symmetric quadrature centroid");}
    for(const float spacing:{.125f,.25f}) {
        auto particles=Pool(spacing,{.4f,-.2f,.1f},{0,0,0},true);
        FluidHydrostaticField field;field.Build(particles,1000);
        BodyTransform pose;pose.position={0,0,0};
        auto half=EvaluateFluidHydrostatics(field,box,pose,gravity);
        Near(half.fraction,.5,2e-6,"half immersion despite particle exclusion");
        Near(half.buoyancyForce.y,4905,0.02,"half immersion Archimedes force");
        Near(half.submergedCentroid.y,-.25,2e-6,"half box immersed centroid");
        VectorNear(half.fluidVelocity,{.4f,-.2f,.1f},2e-6f,"moving liquid velocity");
        VectorNear(half.buoyancyTorque,glm::vec3(0),.001f,"symmetric load has no torque");
        Near(500*gravity.value.y+half.buoyancyForce.y,0,.02,"half-density equilibrium net force");
        pose.position.y=-.75f;const auto submerged=EvaluateFluidHydrostatics(field,box,pose,gravity);
        Near(submerged.fraction,1,2e-6,"fully submerged hole filled by envelope");
        Near(1000*gravity.value.y+submerged.buoyancyForce.y,0,.03,"neutral body zero net force");
        Check(2000*gravity.value.y+submerged.buoyancyForce.y< -9800,"twice density sinks");
        std::cout<<"pool spacing="<<spacing<<" particles="<<particles.size()<<" half_fraction="<<half.fraction<<" half_force="<<half.buoyancyForce.y<<" neutral_net_force="<<1000*gravity.value.y+submerged.buoyancyForce.y<<'\n';
        const auto null=EvaluateFluidHydrostatics(field,box,pose,zero);VectorNear(null.buoyancyForce,glm::vec3(0),0,"zero gravity no buoyancy");
        for(auto& p:particles)p.acceleration=gravity.value;
        field.Build(particles,1000);const auto falling=EvaluateFluidHydrostatics(field,box,pose,gravity);
        VectorNear(falling.buoyancyForce,glm::vec3(0),.01f,"common freefall has no hydrostatic force");
        VectorNear(falling.fluidAcceleration,gravity.value,2e-6f,"latest bulk acceleration not delayed");
        for(auto& p:particles)p.acceleration={0,0,0};
        const glm::quat rotation=glm::angleAxis(1.137f,glm::normalize(glm::vec3(1,2,-3)));const glm::vec3 shift(10,-7,3);
        Rotate(particles,rotation,shift);field.Build(particles,1000);
        pose.position=shift;pose.rotation=rotation;const Uniform rotatedGravity(rotation*gravity.value);
        const auto rotated=EvaluateFluidHydrostatics(field,box,pose,rotatedGravity);
        Near(rotated.fraction,half.fraction,2e-5,"rotated translated immersion covariance");
        VectorNear(rotated.buoyancyForce,rotation*half.buoyancyForce,.2f,"rotated translated force covariance");
        VectorNear(rotated.submergedCentroid,rotation*half.submergedCentroid+shift,2e-5f,"rotated translated centroid covariance");
        VectorNear(rotated.fluidVelocity,rotation*half.fluidVelocity,3e-6f,"rotated velocity covariance");
    }
    FluidHydrostaticField field;auto particles=Pool(.25f);field.Build(particles,1000);BodyTransform pose;
    const auto halfSphere=EvaluateFluidHydrostatics(field,sphere,pose,gravity);
    Near(halfSphere.fraction,.5,2e-6,"antipodal sphere hemisphere volume");
    // Exact hemisphere centroid -3r/8; quadrature is finite and its footprint is approximate.
    Near(halfSphere.submergedCentroid.y,-3*.5/8,.012,"sphere hemisphere centroid approximation");
    for(float length:{.2f,static_cast<float>(.2*std::cbrt(100.0))}){
        const auto scaled=MakeBoxFluidVolume(glm::vec3(length*.5f));const auto result=EvaluateFluidHydrostatics(field,scaled,pose,gravity);
        const double volume=static_cast<double>(length)*length*length;
        Near(result.fraction,.5,2e-6,"mass-scale unchanged immersion");
        Near(result.buoyancyForce.y,1000*9.81*.5*volume,1e-4+volume*.02,"100x volume force scaling");
        std::cout<<"scale body_volume="<<volume<<" neutral_mass="<<1000*volume<<" force="<<result.buoyancyForce.y<<'\n';
    }
    // Asymmetric shape is wholly submerged: analytic union centroid x=.5.
    const auto offset=MakeCompoundFluidVolume({{{0,0,0},{.5f,.5f,.5f}},{{1,0,0},{.5f,.5f,.5f}}});
    pose.position={-.5f,-1,0};const auto load=EvaluateFluidHydrostatics(field,offset,pose,gravity);
    Near(load.buoyancyForce.y,19620,.06,"compound total hydrostatic force");Near(load.buoyancyTorque.z,9810,.04,"compound off-COM hydrostatic torque");
    std::vector<bool> exclude(particles.size(),true);field.Build(particles,1000,&exclude);
    Near(EvaluateFluidHydrostatics(field,box,pose,gravity).fraction,0,0,"explicit cavity exclusion removes its field");
    Check(field.ParticleCount()==0,"excluded particles not retained");
    bool rejected=false;try{std::vector<bool> bad(1);field.Build(particles,1000,&bad);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"invalid exclusion mask rejected");
    // Distinct liquid layers do not fill the air gap between them.
    particles={{{0,-1,0},{0,-1,0},{0,0,0},8,{0,0,0}},{{0,1,0},{0,1,0},{0,0,0},8,{0,0,0}}};field.Build(particles,1000);
    Near(field.Query({0,0,0},{0,1,0},.1f,.3f,{1,0,0}).fraction,0,0,"disconnected vertical layers retain air gap");
    const auto before=field.Query({0,-1,0},{0,1,0},.05f,.3f,{1,0,0});for(int i=0;i<20;++i)Near(field.Query({0,-1,0},{0,1,0},.05f,.3f,{1,0,0}).fraction,before.fraction,0,"repeated deterministic query");
    // Independent constant-field witness: both contributing particles lie
    // outside the former spherical motion kernel (reach=.1+2*.2=.5), while
    // the finite column envelope [-.7,.7] covers the whole sample [-.3,.3].
    particles={{{0,-.6f,0},{0,-.6f,0},{.4f,-.2f,.1f},8,gravity.value},
               {{0, .6f,0},{0, .6f,0},{.4f,-.2f,.1f},8,gravity.value}};
    field.Build(particles,1000);
    const auto wideColumn=field.Query({0,0,0},{0,1,0},.3f,.1f,{1,0,0});
    Near(wideColumn.fraction,1,2e-6,"wet envelope has finite column support");
    VectorNear(wideColumn.velocity,{.4f,-.2f,.1f},2e-6f,"wide wet envelope preserves common velocity");
    VectorNear(wideColumn.acceleration,gravity.value,2e-6f,"wide wet envelope preserves common gravity acceleration");
    // Occupancy is full on both sides of this row-entry event. Local bulk
    // flow interpolation must not jump merely because another interval first
    // touches a quadrature footprint by an infinitesimal amount.
    particles={{{0,0,0},{0,0,0},{0,0,0},8,{0,0,0}},{{0,.2f,0},{0,.2f,0},{0,1,0},8,{0,-9.81f,0}}};field.Build(particles,1000);
    const auto low=field.Query({0,.07499f,0},{0,1,0},.025f,.3f,{1,0,0});
    const auto high=field.Query({0,.07501f,0},{0,1,0},.025f,.3f,{1,0,0});
    Near(low.fraction,1,1e-6,"lower row-entry occupancy");Near(high.fraction,1,1e-6,"upper row-entry occupancy");
    Check(glm::length(high.acceleration-low.acceleration)<.001f,"bulk acceleration spatial continuity");
    Check(glm::length(high.velocity-low.velocity)<.0001f,"bulk velocity spatial continuity");
    std::cout<<"row_entry fraction_low="<<low.fraction<<" fraction_high="<<high.fraction<<" acceleration_low="<<low.acceleration.y<<" acceleration_high="<<high.acceleration.y<<" acceleration_jump="<<glm::length(high.acceleration-low.acceleration)<<" velocity_jump="<<glm::length(high.velocity-low.velocity)<<'\n';
    // Equal-area, equally distant resolved columns have an independent
    // occupied-volume mean. A union across different lateral columns instead
    // reports the tallest column everywhere.
    particles={{{-.4f,-.1f,0},{-.4f,-.1f,0},{0,0,0},8,{0,0,0}},
               {{ .4f,-.1f,0},{ .4f,-.1f,0},{0,0,0},8,{0,0,0}},
               {{ .4f, .1f,0},{ .4f, .1f,0},{0,0,0},8,{0,0,0}}};
    field.Build(particles,1000);
    const auto twoColumns=field.Query({0,0,0},{0,1,0},.2f,.3f,{1,0,0});
    Near(twoColumns.fraction,.75,2e-6,"two columns mean occupied volume");
    particles.clear();
    const glm::vec3 columns[3]={{-.4f,0,0},{.4f,0,0},{0,0,.4f}};
    for(int c=0;c<3;++c)for(int row=0;row<=c;++row){const glm::vec3 p=columns[c]+glm::vec3(0,-.2f+.2f*row,0);particles.push_back({p,p,{0,0,0},8,{0,0,0}});}
    field.Build(particles,1000);const auto threeColumns=field.Query({0,0,0},{0,1,0},.3f,.3f,{1,0,0});
    Near(threeColumns.fraction,2./3.,2e-6,"three columns mean occupied volume");
    const glm::quat columnRotation=glm::angleAxis(.91f,glm::normalize(glm::vec3(-2,3,1)));
    const glm::vec3 columnTranslation(7,-4,2);Rotate(particles,columnRotation,columnTranslation);field.Build(particles,1000);
    const auto rotatedColumns=field.Query(columnTranslation,columnRotation*glm::vec3(0,1,0),.3f,.3f,columnRotation*glm::vec3(1,0,0));
    Near(rotatedColumns.fraction,2./3.,2e-5,"rotated translated column volume mean");
    std::cout<<"column_mean two_observed="<<twoColumns.fraction<<" two_expected=.75 three_observed="<<threeColumns.fraction<<" three_expected="<<2./3.<<'\n';
    // Manufactured background motion: a local wall-disturbed column above
    // a displaced solid must not become that solid's bulk-flow acceleration.
    // Exterior resolved columns all have one known common free-fall vector.
    {
        auto background=Pool(.25f,{.4f,-.2f,.1f},gravity.value);
        for(auto& p:background)if(std::abs(p.position.x)<.5f&&std::abs(p.position.z)<.5f&&p.position.y>-.5f){
            p.acceleration+=glm::vec3(20,100,-30);p.velocity+=glm::vec3(4,10,-6);
        }
        FluidHydrostaticField exterior;exterior.Build(background,1000);
        BodyTransform submergedPose;submergedPose.position={0,-1,0};
        const auto backgroundLoad=EvaluateFluidHydrostatics(exterior,box,submergedPose,gravity);
        Near(backgroundLoad.fraction,1,2e-6,"background fixture is fully immersed");
        VectorNear(backgroundLoad.fluidAcceleration,gravity.value,2e-6f,"solid contact shadow does not redefine exterior bulk acceleration");
        VectorNear(backgroundLoad.fluidVelocity,{.4f,-.2f,.1f},2e-6f,"solid contact shadow does not redefine exterior bulk velocity");
        VectorNear(backgroundLoad.buoyancyForce,glm::vec3(0),.01f,"common-fall background has no self-induced hydrostatic force");
        const glm::quat turn=glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3)));
        Rotate(background,turn,{3,-2,4});exterior.Build(background,1000);
        submergedPose.position=turn*submergedPose.position+glm::vec3(3,-2,4);submergedPose.rotation=turn;
        const Uniform turnedGravity(turn*gravity.value);
        VectorNear(EvaluateFluidHydrostatics(exterior,box,submergedPose,turnedGravity).fluidAcceleration,
            turn*backgroundLoad.fluidAcceleration,2e-5f,"nonuniform contact-shadow reconstruction rotates with geometry");
    }
    // Captured ordinary production pool after the unchanged 240-step settling
    // phase. The box spans y=.4..1; exterior surface median is 1.12776m.
    // Its neutral-drift gate requires essentially full support: a 1% deficit
    // alone gives ~.172m in 4s under the fixture's drag rate 2/s.
    const auto snapshotRoot=std::filesystem::path(__FILE__).parent_path().parent_path()/"docs/evidence/stabilization/ftft9/body-development/resume-neutral";
    std::ifstream snapshot(snapshotRoot/"neutral_020_release_particles.csv");
    Check(snapshot.is_open(),"recorded settled-pool particle snapshot available");
    std::string line;std::getline(snapshot,line);particles.clear();
    while(std::getline(snapshot,line)) {
        std::replace(line.begin(),line.end(),',',' ');std::istringstream values(line);
        glm::vec3 p,v,a;float mass;values>>p.x>>p.y>>p.z>>v.x>>v.y>>v.z>>a.x>>a.y>>a.z>>mass;
        if(!values)throw std::runtime_error("invalid recorded particle snapshot");
        particles.push_back({p,p,v,mass,a});
    }
    Check(particles.size()==752,"recorded settled-pool particle count unchanged");
    field.Build(particles,1000);pose={};pose.position={0,.7f,0};
    const auto actualBox=MakeBoxFluidVolume(glm::vec3(.3f));
    const auto actual=EvaluateFluidHydrostatics(field,actualBox,pose,gravity);
    Check(actual.fraction>=.99f && actual.fraction<=1.000001f,"settled production bulk provides near-full neutral support");
    std::cout<<"settled_snapshot particles="<<particles.size()<<" fraction="<<actual.fraction<<" force_y="<<actual.buoyancyForce.y<<" bulk_acceleration_y="<<actual.fluidAcceleration.y<<'\n';
    const glm::quat snapshotRotation=glm::angleAxis(1.137f,glm::normalize(glm::vec3(1,2,-3)));
    const glm::vec3 snapshotShift(10,-7,3);Rotate(particles,snapshotRotation,snapshotShift);field.Build(particles,1000);
    pose.position=snapshotRotation*pose.position+snapshotShift;pose.rotation=snapshotRotation;
    const Uniform snapshotGravity(snapshotRotation*gravity.value);
    const auto rotatedActual=EvaluateFluidHydrostatics(field,actualBox,pose,snapshotGravity);
    Near(rotatedActual.fraction,actual.fraction,2e-5,"recorded bulk rotation translation covariance");
    VectorNear(rotatedActual.buoyancyForce,snapshotRotation*actual.buoyancyForce,.2f,"recorded bulk force covariance");
    // Explicit approximation limit: unlike distant disconnected layers above,
    // an air pocket bounded by particles inside the local reconstruction
    // window is filled. This is a hydrostatic envelope, not multiphase CFD.
    particles={{{0,-.3f,0},{0,-.3f,0},{0,0,0},8,{0,0,0}},{{0,.3f,0},{0,.3f,0},{0,0,0},8,{0,0,0}}};field.Build(particles,1000);
    const auto unresolvedPocket=field.Query({0,0,0},{0,1,0},.05f,.3f,{1,0,0});
    std::cout<<"approximation_limit near_air_pocket_fraction="<<unresolvedPocket.fraction<<" physical_point_occupancy=0 local_envelope_occupancy=1\n";
    // Regression: a motion-donor radius is not a physical water footprint.
    // Previously one distant column normalized its own tiny weight to 100%
    // wetness, and applied buoyancy to any sufficiently nearby dry body.
    particles={{{0,0,0},{0,0,0},{.4f,-.2f,.1f},125,{0,0,0}}};
    field.Build(particles,1000);
    for(float x:{.5f,1.f,2.f,3.f,4.f}) {
        const auto dry=field.Query({x,0,0},{0,1,0},.0625f,3.6f,{1,0,0});
        Near(dry.fraction,0,0,"distant donor cannot create lateral wet support");
        VectorNear(dry.acceleration,{0,0,0},0,"dry spatial query publishes no water acceleration");
    }
    for(float half:{.25f,.45f}) {
        const auto dryBox=MakeBoxFluidVolume(glm::vec3(half));
        BodyTransform dryPose;dryPose.position={1.1f,0,0};
        const auto dry=EvaluateFluidHydrostatics(field,dryBox,dryPose,gravity);
        Near(dry.fraction,0,0,"generic dry box has no false displaced volume");
        VectorNear(dry.buoyancyForce,{0,0,0},0,"generic dry box has no phantom buoyancy");
        VectorNear(dry.buoyancyTorque,{0,0,0},0,"generic dry box has no phantom torque");
        auto turned=particles;
        const auto turn=glm::angleAxis(.73f,glm::normalize(glm::vec3(1,-2,3)));
        const glm::vec3 shift(7,-3,9);Rotate(turned,turn,shift);
        FluidHydrostaticField rotatedField;rotatedField.Build(turned,1000);
        dryPose.position=turn*dryPose.position+shift;dryPose.rotation=turn;
        const Uniform turnedGravity(turn*gravity.value);
        VectorNear(EvaluateFluidHydrostatics(rotatedField,dryBox,dryPose,turnedGravity).buoyancyForce,
                   {0,0,0},0,"dry gap stays dry under rotated gravity and translation");
    }
    // Positive controls at the same coarse spacing: the geometric guard must
    // retain water that really overlaps a query, including its free surface.
    Near(field.Query({0,0,0},{0,1,0},.0625f,3.6f,{1,0,0}).fraction,
         1,2e-6,"coarse particle retains actual local query support");
    // An immersed solid needs exterior donors: a single particle inside the
    // solid is excluded by the existing solid-occlusion contract, not water
    // displaced around a body. Use a real surrounding coarse pool here.
    field.Build(Pool(.5f),1000);
    const auto smallBox=MakeBoxFluidVolume(glm::vec3(.05f));
    BodyTransform smallPose;smallPose.position.y=-1;
    const auto wet=EvaluateFluidHydrostatics(field,smallBox,smallPose,gravity);
    Near(wet.fraction,1,2e-6,"coarse particle footprint retains genuine full immersion");
    Near(wet.buoyancyForce.y,9.81,.0001,"coarse full immersion keeps displaced-water weight");
    smallPose.position.y=0;
    const auto surface=EvaluateFluidHydrostatics(field,smallBox,smallPose,gravity);
    Near(surface.fraction,.5,2e-6,"coarse footprint retains genuine half immersion");
    Near(surface.buoyancyForce.y,4.905,.0001,"coarse half immersion keeps displaced-water weight");
    // Two disconnected patches must not join their convex envelopes across a
    // dry player/body. Keep the lower-level column interpolation checks above.
    particles={{{-1.1f,0,0},{-1.1f,0,0},{0,0,0},125,{0,0,0}},
               {{ 1.1f,0,0},{ 1.1f,0,0},{0,0,0},125,{0,0,0}}};
    field.Build(particles,1000);
    Near(field.Query({0,0,0},{0,1,0},.1f,3.6f,{1,0,0},nullptr,true).fraction,
         0,0,"disconnected pools do not immerse a dry point between them");
    const auto between=EvaluateFluidHydrostatics(field,MakeBoxFluidVolume(glm::vec3(.25f)),{},gravity);
    VectorNear(between.buoyancyForce,{0,0,0},0,"disconnected pools do not support a dry box");
    const auto dryPlayer=EvaluateFluidHydrostatics(field,MakeBoxFluidVolume(glm::vec3(.1f)),{},gravity,
                                                FluidVolumeGeometry::NonDisplacingVolume);
    Near(dryPlayer.fraction,0,0,"disconnected pools do not trigger false swimming");
    Near(field.Query({-1.1f,0,0},{0,1,0},.1f,3.6f,{1,0,0},nullptr,true).fraction,
         1,2e-6,"connected query still recognizes actual water in an isolated patch");
    std::cout<<"SUMMARY checks="<<checks<<" failures="<<failures<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<'\n';
    return failures?1:0;
}
