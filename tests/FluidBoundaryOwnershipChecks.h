#pragma once
#include "FluidDisplacementTracking.h"

void FluidBoundaryOwnershipChecks() {
    const BodyHandle owner{37,901}, foreign{38,901}, recycled{37,902};
    const auto volume=MakeBoxFluidVolume(glm::vec3(.3f));
    BodyTransform pose;
    const Uniform gravity({0,-9.81f,0});
    std::vector<FluidParticle> source,cut,voidDomain;
    const float spacing=.0625f;
    for(int z=0;z<32;++z)for(int x=0;x<32;++x)for(int y=0;y<32;++y) {
        const glm::vec3 p(-1+(x+.5f)*spacing,-(y+.5f)*spacing,-1+(z+.5f)*spacing);
        FluidParticle particle{p,p,{.4f,0,0},1000*spacing*spacing*spacing,{0,0,0}};
        source.push_back(particle);
        if(!glm::all(glm::lessThan(glm::abs(p),glm::vec3(.3f))))cut.push_back(particle);
        if(!glm::all(glm::lessThan(glm::abs(p),glm::vec3(.3125f))))voidDomain.push_back(particle);
    }
    Check(cut.size()==voidDomain.size(),"twin domains retain the same material particle count");
    FluidHydrostaticField field;
    field.Build(cut,1000);
    const auto sample=[&](BodyHandle id,const FluidVolumeQuadrature& shape,const BodyTransform& transform) {
        return EvaluateFluidHydrostatics(field,shape,transform,gravity,FluidVolumeGeometry::DisplacedSolid,id);
    };
    Near(sample(owner,volume,pose).fraction,0,0,"metadata-free original contact gap remains conservative");
    field.RetainDisplacedBoundary(owner,volume,pose,source);
    Near(sample(owner,volume,pose).fraction,.5,.001,"owned pre-displacement water boundary retains half immersion");
    VectorNear(sample(owner,volume,pose).fluidVelocity,{.4f,0,0},.00001f,
               "retained geometry keeps actual exterior bulk velocity");
    Near(sample(foreign,volume,pose).fraction,0,0,"foreign body cannot borrow another solid's displaced domain");
    Near(sample(recycled,volume,pose).fraction,0,0,"replacement world cannot inherit old owner boundary");
    Near(sample({},volume,pose).fraction,0,0,"ordinary queries do not opt into owned displacement implicitly");
    const auto resized=MakeBoxFluidVolume(glm::vec3(.29f));
    Near(sample(owner,resized,pose).fraction,0,0,"shape replacement invalidates retained ownership");
    BodyTransform moved=pose;moved.position.y=3;
    Near(sample(owner,volume,moved).fraction,0,0,"retained water stays world anchored after teleport");
    auto invalid=source;invalid[0].position.x=std::numeric_limits<float>::quiet_NaN();
    bool rejected=false;
    try{field.RetainDisplacedBoundary(owner,volume,pose,invalid);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"invalid boundary update rejected");
    Near(sample(owner,volume,pose).fraction,.5,.001,"failed publication preserves previous boundary");
    field.RetainDisplacedBoundary(owner,volume,pose,voidDomain);
    Near(sample(owner,volume,pose).fraction,0,0,"explicit dry twin remains dry with identical physical particles");
    // Geometry data cannot add an artificial acceleration or velocity source.
    for(auto& p:source){p.velocity={100,200,300};p.acceleration={50,60,70};}
    field.RetainDisplacedBoundary(owner,volume,pose,source);
    const auto restored=sample(owner,volume,pose);
    VectorNear(restored.fluidVelocity,{.4f,0,0},.00001f,"virtual domain velocities are not material velocity");
    VectorNear(restored.fluidAcceleration,{0,0,0},0,"virtual domain accelerations cannot launch a solid");
    Near(restored.buoyancyForce.y,1000*volume.totalVolume*.5*9.81,.01,
         "owned boundary receives displaced-water weight once");
    field.Build(cut,1000);
    Near(sample(owner,volume,pose).fraction,0,0,"field rebuild expires all old boundary snapshots");
    FluidDisplacementMarker invented;
    invented.owner=owner;invented.position={0,0,0};invented.halfWidth=spacing*.5f;
    invented.materialPoint={.34375f,-.03125f,-.03125f};
    invented.contactPoint={.3f,-.03125f,-.03125f};
    field.RetainDisplacementCells(owner,volume,pose,{invented});
    Near(sample(owner,volume,pose).fraction,0,0,"unproven cut witness cannot bridge the original real support gap");
    invented.halfWidth=.2f;
    field.RetainDisplacementCells(owner,volume,pose,{invented});
    Near(sample(owner,volume,pose).fraction,0,0,"inflated cut cell cannot replace its actual material support size");


    FluidSettings settings;settings.substeps=1;settings.densityIterations=0;settings.velocitySmoothing=0;
    FluidWorld fluid(settings);const Uniform zero({0,0,0});
    fluid.AddParticle({0,0,0},{0,0,0},1.953125f);
    FluidBoxCollider box{owner,{}, {}, {.3f,.3f,.3f}};
    fluid.Step(.01f,zero,{box});
    Check(fluid.DisplacementMarkers().size()==1,"one bounded boundary marker per actual particle");
    Check(FluidBoundaryDetail::SameOwner(fluid.DisplacementMarkers()[0].owner,owner),
          "real box projection records full owner identity");
    Near(glm::length(fluid.DisplacementMarkers()[0].position),0,.00001,"contact retains pre-solid geometry");
    Near(fluid.GetDiagnostics().totalMass,1.953125,0,"boundary capture adds no fluid mass");
    Near(glm::length(fluid.Particles()[0].velocity),0,0,"passive boundary capture adds no velocity");
    fluid.Step(.01f,zero,{box});
    Check(fluid.DisplacementMarkers()[0].owner.IsValid(),"resting contact retains boundary rather than forgetting each step");
    fluid.Step(.01f,zero,{});
    Check(!fluid.DisplacementMarkers()[0].owner.IsValid(),"contact removal expires boundary geometry");
    FluidDisplacementMarker cutCell;
    const std::vector<FluidBoundarySolid> solids{{owner,{},glm::vec3(.3f),0}};
    UpdateFluidDisplacementMarker(cutCell,{.318f,0,0},{-.002f,0,0},
        owner,solids,.018f,.03125f);
    Check(cutCell.owner.IsValid(),"actual contact retains a cut support cell even with its centre outside solid");
    UpdateFluidDisplacementMarker(cutCell,{.4f,0,0},{0,0,0},
        {},solids,.018f,.03125f);
    Check(!cutCell.owner.IsValid(),"departing actual particle cannot leave ghost water attached to a solid");
    fluid.Clear();
    Check(fluid.DisplacementMarkers().empty(),"Clear removes all owned boundary state");
}

void OwnedCurvedBoundaryChecks() {
    constexpr double pi=3.14159265358979323846;
    constexpr float radius=.3f;
    const double r=radius,s=2*r*r/(std::sqrt(1+r*r)+1);
    const double sphereReference=(2*pi*((r*r*r-std::pow(r*r-s,1.5))/3-s*s/8))/(4*pi*r*r*r/3);
    const BodyHandle owner{37,901};
    for(bool sphere : {false,true}) {
        const auto turn=sphere?glm::quat(1,0,0,0):glm::angleAxis(.55f,glm::normalize(glm::vec3(1,0,2)));
        double reference=sphere?sphereReference:0;
        if(!sphere) {
            std::uint64_t wet=0;
            constexpr int n=96;
            for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
                const auto p=glm::dquat(turn)*((glm::dvec3(2*x+1,2*y+1,2*z+1)/double(n)-1.)*double(radius));
                wet+=p.y<-.5*(p.x*p.x+p.z*p.z);
            }
            reference=double(wet)/(n*n*n);
        }
        for(float spacing : {.25f,.125f}) {
            float fractions[2]{};
            for(int frame=0;frame<2;++frame) {
                const auto q=frame?glm::angleAxis(.63f,glm::normalize(glm::vec3(1,-2,3))):glm::quat(1,0,0,0);
                const glm::vec3 offset=frame?glm::vec3(1,-2,3):glm::vec3(0);
                std::vector<FluidParticle> source,cut;
                const int n=static_cast<int>(std::lround(2/spacing));
                for(int z=0;z<n;++z)for(int x=0;x<n;++x)for(int row=0;row<n;++row) {
                    const float px=-1+(x+.5f)*spacing,pz=-1+(z+.5f)*spacing;
                    const glm::vec3 local(px,-.5f*(px*px+pz*pz)-(row+.5f)*spacing,pz);
                    const auto p=offset+q*local;
                    FluidParticle particle{p,p,glm::vec3(0),1000*spacing*spacing*spacing,glm::vec3(0)};
                    source.push_back(particle);
                    const auto bodyPoint=glm::conjugate(turn)*local;
                    const bool inside=sphere?glm::dot(bodyPoint,bodyPoint)<radius*radius:
                        glm::all(glm::lessThan(glm::abs(bodyPoint),glm::vec3(radius)));
                    if(!inside)cut.push_back(particle);
                }
                const auto volume=sphere?MakeSphereFluidVolume(radius):MakeBoxFluidVolume(glm::vec3(radius));
                BodyTransform pose;pose.position=offset;pose.rotation=q*turn;
                FluidHydrostaticField field;field.Build(cut,1000);
                field.RetainDisplacedBoundary(owner,volume,pose,source);
                const Uniform gravity(q*glm::vec3(0,-9.81f,0));
                fractions[frame]=EvaluateFluidHydrostatics(field,volume,pose,gravity,
                    FluidVolumeGeometry::DisplacedSolid,owner).fraction;
                Near(fractions[frame],reference,.10,"owned curved-body domain agrees with independent geometry");
            }
            Near(fractions[0],fractions[1],.001,"owned curved-body geometry follows global frame changes");
        }
    }
}

void NativeOwnedDryRegionChecks() {
    const BodyHandle owner{73,401};
    for(int frame=0;frame<2;++frame)for(bool layers : {false,true}) {
        const auto q=frame?glm::angleAxis(.63f,glm::normalize(glm::vec3(1,-2,3))):glm::quat(1,0,0,0);
        const glm::vec3 offset=frame?glm::vec3(1,-2,3):glm::vec3(0);
        FluidSettings settings;settings.substeps=1;settings.densityIterations=0;settings.velocitySmoothing=0;
        FluidWorld fluid(settings);
        const float spacing=layers?.25f:.125f;
        if(layers) {
            for(int z=-3;z<=3;++z)for(int x=-3;x<=3;++x)for(int y=-4;y<4;++y) {
                const float h=(y+.5f)*spacing;
                if(std::abs(h)<.5f)continue;
                fluid.AddParticle(offset+q*glm::vec3(x*spacing,h,z*spacing),glm::vec3(0),1000*spacing*spacing*spacing);
            }
        } else {
            for(int z=0;z<16;++z)for(int x=0;x<16;++x)for(int y=0;y<16;++y) {
                const float px=-1+(x+.5f)*spacing,pz=-1+(z+.5f)*spacing;
                if(std::abs(px)<.375f&&std::abs(pz)<.375f)continue;
                fluid.AddParticle(offset+q*glm::vec3(px,-(y+.5f)*spacing,pz),glm::vec3(0),1000*spacing*spacing*spacing);
            }
        }
        const glm::vec3 half=layers?glm::vec3(.1f,1.1f,.1f):glm::vec3(.3f);
        const BodyTransform pose{offset,q};
        const Uniform zero({0,0,0});
        fluid.Step(1.f/60,zero,{{owner,pose,pose,half}});
        std::vector<FluidDisplacementMarker> cells;
        for(const auto& cell:fluid.DisplacementMarkers())if(cell.owner.IsValid())cells.push_back(cell);
        if(layers)Check(!cells.empty(),"native rod has positive water contacts on both sides of the air layer");
        else Check(cells.empty(),"native dry-hole body acquires no invented displacement ownership");
        const auto volume=MakeBoxFluidVolume(half);
        FluidHydrostaticField field;field.Build(fluid.Particles(),1000);
        if(!cells.empty())field.RetainDisplacementCells(owner,volume,pose,cells);
        const FluidSolidQuery solid{volume,pose,owner};
        const auto result=field.Query(offset,q*glm::vec3(0,1,0),.1f,1.6f,
            field.ReconstructionTangent(q*glm::vec3(0,1,0),q*glm::vec3(1,0,0)),&solid,true);
        Near(result.fraction,0,0,layers?"native owned cut cells cannot fill the resolved vertical air layer":
             "native capture preserves the real lateral dry gap");
    }
}
