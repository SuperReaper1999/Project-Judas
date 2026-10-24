#include "Deformable.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "SaveArchive.h"
#include "UniformGravity.h"
#include "RadicalGravity.h"
#include <cstdio>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
namespace {int failures=0,checks=0;void check(bool ok,const char* message){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",message);} }
int main(){
    std::string error;auto sheet=std::make_shared<DeformableAsset>(MakeDeformableSheet(5,5,1,1,2));auto block=std::make_shared<DeformableAsset>(MakeDeformableBlock({2,2,2},{1,1,1}));
    check(sheet->nodes.size()==25&&sheet->render.vertices.size()==81,"denser cloth render binding");check(block->solids.size()==48,"block six tetrahedra per cell");
    double volume=0;for(auto& t:block->solids)volume+=t.volume;check(std::abs(volume-1)<1e-12,"independent unit cube volume reference");
    auto encoded=EncodeDeformableAsset(*sheet);DeformableAsset decoded;check(DecodeDeformableAsset({encoded.begin(),encoded.end()},decoded,error)&&decoded.nodes==sheet->nodes&&decoded.binding.size()==81,"asset versioned roundtrip");
    auto bad=*block;bad.tetrahedra[0].x=bad.tetrahedra[0].y;check(!PrepareDeformableAsset(bad,error),"degenerate tetrahedron rejects");
    PhysicsWorld physics;physics.Init();UniformGravity zero({0,0,0});DeformableSettings cfg;cfg.material.damping=0;cfg.selfContact=false;DeformableInstance cloth,solid,independent;
    cloth.Initialize(sheet,cfg,glm::dmat4(1),1);cfg.material.density=1000;solid.Initialize(block,cfg,glm::dmat4(1),2);independent.Initialize(block,cfg,glm::dmat4(1),3);
    for(int step=0;step<30;++step){cloth.Step(1./60,physics,zero,{});solid.Step(1./60,physics,zero,{});}double drift=0;for(size_t n=0;n<block->nodes.size();++n)drift=std::max(drift,glm::length(solid.positions[n]-block->nodes[n]));check(drift<1e-10,"strain/volume rest state is stationary");
    check(std::abs(cloth.Mass()-.4)<1e-12&&std::abs(solid.Mass()-1000)<1e-9,"density derives surface and volume masses");
    solid.Impulse("",{1,0,0});solid.Step(1./60,physics,zero,{});check(solid.positions[0].x!=independent.positions[0].x&&independent.velocities[0]==glm::dvec3(0),"shared immutable solid independent motion");
    Scene scene,round;auto& o=scene.CreateObject("cloth");o.deformable=cfg;std::string text;SaveSceneToString(scene,text);check(LoadSceneFromString(text,round,error)&&ScenesEqual(scene,round),"normal authored component roundtrip");
    MeshData rendered;cloth.MapRender(.5,rendered);check(rendered.vertices.size()==81&&glm::length(rendered.vertices[40].normal)>.99,"stored weights map render surface and normals");
    auto hit=cloth.Raycast({0,.5,1},{0,0,-1},2);check(hit.hit&&std::abs(hit.distance-1)<1e-9,"current deformed surface ray query");check(cloth.Impulse(hit.location,{0,0,.05}),"validated location accepts physical impulse");auto stale=hit.location;stale.generation=999;check(!cloth.Impulse(stale,{1,0,0}),"stale deformable location rejects");
    SaveArchive saved;solid.Persist(saved);DeformableInstance restored;restored.Initialize(block,cfg,glm::dmat4(1),4);SaveArchive load(saved.bytes);restored.Persist(load);load.Finish();check(restored.positions==solid.positions&&restored.velocities==solid.velocities,"authoritative state persists with fresh generation");
    auto floor=physics.CreateStaticBox({0,-.7f,0},{2,.2f,2},.5f,0);UniformGravity down({0,-9.81f,0});for(int step=0;step<120;++step)solid.Step(1./60,physics,down,{});check(solid.error.empty()&&solid.Minimum().y>-.55,"elastic solid contacts ordinary static box");physics.DestroyBody(floor);
    auto patch=std::make_shared<DeformableAsset>(MakeDeformableSheet(7,7,1,1));DeformableInstance driven;DeformableSettings thin;thin.material.damping=0;thin.material.friction=0;thin.selfContact=false;driven.Initialize(patch,thin,glm::dmat4(1),6);
    auto striker=physics.CreateDynamicSphere({0,.5f,.32f},.15f,.8f,0,0);physics.SetLinearVelocity(striker,{0,0,-1});double initialEnergy=.4,maximumEnergy=0;
    for(int step=0;step<45;++step){physics.Step(1.f/60);driven.Step(1./60,physics,zero,{});double energy=.5*physics.GetMass(striker)*glm::dot(physics.GetLinearVelocity(striker),physics.GetLinearVelocity(striker));for(size_t n=0;n<driven.positions.size();++n)energy+=.5*patch->measures[n]*thin.material.density*glm::dot(driven.velocities[n],driven.velocities[n]);maximumEnergy=std::max(maximumEnergy,energy);}
    glm::dvec3 momentum=glm::dvec3(physics.GetLinearVelocity(striker))*double(physics.GetMass(striker));for(size_t n=0;n<driven.positions.size();++n)momentum+=driven.velocities[n]*patch->measures[n]*thin.material.density;
    std::printf("COUPLING momentum=%.9g %.9g %.9g initialEnergy=%.9g maximumKineticEnergy=%.9g\n",momentum.x,momentum.y,momentum.z,initialEnergy,maximumEnergy);
    check(glm::length(momentum-glm::dvec3(0,0,-.8))<2e-5,"two-way contact conserves closed linear momentum");check(maximumEnergy<initialEnergy*1.1,"unforced finite-mass contact has no unexplained kinetic growth");check(glm::length(physics.GetLinearVelocity(striker)-glm::vec3(0,0,-1))>.05f,"normal finite rigid body reacts to cloth");physics.DestroyBody(striker);
    DeformableSettings yielding;yielding.material.density=100;yielding.material.shearCompliance=.002;yielding.material.volumeCompliance=.00002;yielding.material.damping=1;yielding.material.yieldStrain=.06;yielding.material.plasticRate=1;yielding.material.maximumPlasticStrain=.4;yielding.selfContact=false;DeformableAttachment pin;pin.group="left";yielding.attachments={pin};DeformableTarget anchor;anchor.valid=true;DeformableInstance dent,elastic;dent.Initialize(block,yielding,glm::dmat4(1),7);elastic.Initialize(block,yielding,glm::dmat4(1),8);
    for(int step=0;step<60;++step){dent.Force("right",{0,0,80});dent.Step(1./60,physics,zero,{anchor});elastic.Force("right",{0,0,1});elastic.Step(1./60,physics,zero,{anchor});}
    double dentRest=0,elasticRest=0;for(size_t i=0;i<block->solids.size();++i)for(int k=0;k<3;++k){dentRest=std::max(dentRest,glm::length(dent.plasticRest[i][k]-block->solids[i].rest[k]));elasticRest=std::max(elasticRest,glm::length(elastic.plasticRest[i][k]-block->solids[i].rest[k]));}
    std::printf("PLASTIC retainedRest=%.9g belowYieldRest=%.9g jacobian=%.9g maximumStrain=%.9g\n",dentRest,elasticRest,dent.stats.minimumJacobian,dent.stats.maximumStrain);
    check(dentRest>.001&&dent.error.empty(),"physical force induces bounded irreversible material rest flow");check(elasticRest<1e-10,"below-yield physical force leaves rest state unchanged");
    for(int step=0;step<120;++step)dent.Step(1./60,physics,zero,{anchor});
    double lasting=0;for(size_t n=0;n<block->nodes.size();++n)lasting=std::max(lasting,glm::length(dent.positions[n]-block->nodes[n]));check(lasting>.001,"deformation remains after unloading without a script pose swap");
    bool preserved=true;for(size_t i=0;i<block->solids.size();++i)preserved&=std::abs(glm::determinant(dent.plasticRest[i])-6*block->solids[i].volume)<1e-8;check(preserved,"plastic rule preserves each rest volume");
    // Topology-aware seam fixture: six render vertices but four source nodes.
    MeshData seams;seams.vertices.resize(6);seams.indices={0,1,2,3,4,5};seams.sourceVertexIds={0,1,2,0,2,3};const glm::vec3 quad[]={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};for(unsigned i=0;i<6;++i){seams.vertices[i].position=quad[seams.sourceVertexIds[i]];seams.vertices[i].uv={float(i%3),float(i/3)};}DeformableAsset imported;check(ImportDeformableCloth(seams,imported,error)&&imported.nodes.size()==4&&imported.binding.size()==6,"render UV seam shares deliberate source topology");
    seams.sourceVertexIds={0,1,2,3,4,5};check(ImportDeformableCloth(seams,imported,error)&&imported.nodes.size()==6,"coincident separate source layers never position-weld");
    // Rotated rest configuration carries no elastic strain; oblique gravity
    // rotates the same trajectories without using a universal up vector.
    glm::dquat rotation=glm::angleAxis(.71,glm::normalize(glm::dvec3(1,2,3)));auto frame=glm::mat4_cast(rotation);DeformableInstance rotated,original;thin.selfContact=false;original.Initialize(patch,thin,glm::dmat4(1),9);rotated.Initialize(patch,thin,frame,10);UniformGravity oblique(glm::vec3(rotation*glm::dvec3(0,-9.81,0)));
    for(int i=0;i<30;++i){original.Step(1./60,physics,down,{});rotated.Step(1./60,physics,oblique,{});}double equivalent=0;for(size_t i=0;i<patch->nodes.size();++i)equivalent=std::max(equivalent,glm::length(rotation*original.positions[i]-rotated.positions[i]));check(equivalent<1e-6&&rotated.stats.maximumStrain<1e-8,"arbitrary orientation and gravity rotate free motion equivalently");
    RadicalGravity radial({0,0,0},4);DeformableInstance planet;planet.Initialize(block,cfg,glm::translate(glm::dmat4(1),glm::dvec3(5,8,2)),11);planet.Step(1./60,physics,radial,{});check(glm::dot(planet.velocities[0],planet.positions[0])<0,"actual radial gravity pulls deformable toward field centre");
    // Surface crossing: a vertex crosses the INTERIOR of another triangle;
    // no colliding particle centres are close enough to explain the response.
    auto single=std::make_shared<DeformableAsset>();single->nodes={{-1,0,-1},{1,0,-1},{0,0,1}};single->triangles={{0,1,2}};check(PrepareDeformableAsset(*single,error),"independent surface fixture prepares");DeformableInstance face,tip;thin.selfContact=true;thin.material.thickness=.02;face.Initialize(single,thin,glm::dmat4(1),12);tip.Initialize(single,thin,glm::translate(glm::dmat4(1),glm::dvec3(0,.2,0)),13);tip.positions[0]={0,-.2,0};tip.previous[0]={0,.2,0};DeformableInstance::SurfaceContacts(tip,face,1./60);check(tip.stats.contacts>0&&tip.positions[0].y>-.2,"swept vertex/face detects crossing beyond particle-centre contacts");
    auto folded=std::make_shared<DeformableAsset>(*single);folded->nodes.insert(folded->nodes.end(),{{0,.2,0},{.3,.2,0},{0,.2,.3}});folded->triangles.push_back({3,4,5});folded->massWeights.clear();folded->render={};folded->binding.clear();check(PrepareDeformableAsset(*folded,error),"disconnected cloth pieces preserve topology");DeformableInstance folding;folding.Initialize(folded,thin,glm::dmat4(1),14);folding.positions[3].y=-.2;DeformableInstance::SurfaceContacts(folding,folding,1./60);check(folding.stats.contacts>0&&folding.positions[3].y>-.2,"cloth self-contact excludes adjacency but retains folded surface crossing");
    face.settings.collisionMask=0;auto before=tip.positions;DeformableInstance::SurfaceContacts(tip,face,1./60);check(tip.positions==before,"M39 paired deformable mask rejects contact");
    // Prescribed attachment follows the authoritative sampled target; release
    // retains motion, while explicit reset clears presentation and plasticity.
    DeformableSettings pinned=thin;pinned.attachments={pin};auto pinSheet=std::make_shared<DeformableAsset>(MakeDeformableSheet(4,4,1,1));pin.group="top";pinned.attachments={pin};DeformableInstance follower;follower.Initialize(pinSheet,pinned,glm::dmat4(1),15);DeformableTarget moving;moving.valid=true;moving.current=glm::translate(glm::dmat4(1),glm::dvec3(.01,0,0));follower.Step(1./60,physics,zero,{moving});bool follows=true;for(auto n:pinSheet->groups.at("top"))follows&=glm::length(follower.positions[n]-(pinSheet->nodes[n]+glm::dvec3(.01,0,0)))<1e-9;check(follows,"moving group pin samples target at fixed-step boundary");auto motion=follower.velocities[0];check(follower.Release("top")&&motion.x>.1,"release retains physically observed anchor motion");follower.Reset(glm::dmat4(1));check(follower.previous==follower.positions&&follower.velocities[0]==glm::dvec3(0),"explicit reset coherently clears interpolation and velocity");
    DeformableInstance quiet;thin.selfContact=false;quiet.Initialize(patch,thin,glm::dmat4(1),16);for(int i=0;i<75;++i)quiet.Step(1./60,physics,zero,{});check(quiet.sleeping,"quiescent deformable sleeps independently of visibility");quiet.Impulse("",{.1,0,0});check(!quiet.sleeping,"ordinary impulse wakes sleeping deformable");
    // Two practical authored quality settings. Report errors, not a promise of
    // mathematical iteration/timestep independence.
    auto quality=[&](unsigned substeps,unsigned iterations,double dt){DeformableSettings q=yielding;q.material.yieldStrain=0;q.substeps=substeps;q.iterations=iterations;DeformableInstance sample;sample.Initialize(block,q,glm::dmat4(1),20);for(int i=0;i<int(.5/dt);++i){sample.Force("right",{0,0,20});sample.Step(dt,physics,zero,{anchor});}double extension=glm::length(sample.positions.back()-block->nodes.back());std::printf("QUALITY dt=%.8g substeps=%u iterations=%u extension=%.9g maxStrain=%.9g minJ=%.9g\n",dt,substeps,iterations,extension,sample.stats.maximumStrain,sample.stats.minimumJacobian);return extension;};double coarse=quality(4,4,1./60),fine=quality(8,6,1./120);check(std::isfinite(coarse)&&std::isfinite(fine)&&std::abs(coarse-fine)<.2,"practical timestep/quality variants remain in bounded deformation envelope");
    SaveArchive dentSaved;dent.Persist(dentSaved);DeformableInstance dentReload;dentReload.Initialize(block,yielding,glm::dmat4(1),21);SaveArchive dentRead(dentSaved.bytes);dentReload.Persist(dentRead);check(dentReload.positions==dent.positions&&dentReload.plasticRest==dent.plasticRest&&dentReload.settings.attachments.size()==1,"snapshot retains permanent rest change and attachment state");
    MeshData interpolated;auto authoritative=driven.positions;driven.MapRender(.25,interpolated);check(driven.positions==authoritative,"presentation mapping never mutates authoritative simulation");

    // Free finite support receives attachment reaction; it is not a prescribed
    // world/bone target. Its owner body's solver remains the ordinary rigid step.
    auto carrier=physics.CreateDynamicBox({0,2,0},{.1f,.1f,.1f},2,0,0);DeformableInstance supported;DeformableSettings carried=pinned;carried.material.damping=.5;carried.material.friction=0;carried.attachments[0].kind=DeformableAttachment::Kind::Body;carried.attachments[0].target=1;carried.attachments[0].offset={0,-2,0};supported.Initialize(pinSheet,carried,glm::dmat4(1),22);supported.Impulse("",{.1,0,0});double carrierEnergy=0;DeformableTarget carrierTarget;carrierTarget.valid=true;carrierTarget.body=carrier;
    for(int i=0;i<60;++i){auto oldPose=physics.GetTransform(carrier);physics.Step(1.f/60);auto newPose=physics.GetTransform(carrier);carrierTarget.previous=glm::translate(glm::dmat4(1),glm::dvec3(oldPose.position))*glm::mat4_cast(glm::dquat(oldPose.rotation));carrierTarget.current=glm::translate(glm::dmat4(1),glm::dvec3(newPose.position))*glm::mat4_cast(glm::dquat(newPose.rotation));supported.Step(1./60,physics,zero,{carrierTarget});auto omega=physics.GetAngularVelocity(carrier);double energy=physics.GetMass(carrier)*.5*glm::dot(physics.GetLinearVelocity(carrier),physics.GetLinearVelocity(carrier))+.5*glm::dot(omega,physics.GetInertiaWorld(carrier)*omega);for(size_t n=0;n<pinSheet->nodes.size();++n)energy+=.5*pinSheet->measures[n]*carried.material.density*glm::dot(supported.velocities[n],supported.velocities[n]);carrierEnergy=std::max(carrierEnergy,energy);}
    std::printf("DYNAMIC_ATTACHMENT max_energy=%.9g carrier_speed=%.9g\n",carrierEnergy,glm::length(physics.GetLinearVelocity(carrier)));check(glm::length(physics.GetLinearVelocity(carrier))>.001f,"free rigid support receives finite cloth attachment reaction");check(carrierEnergy<.04&&supported.error.empty(),"unforced dynamic attachment remains in bounded energy envelope");physics.DestroyBody(carrier);

    // A drifted pin (node away from its body anchor) must not create angular
    // momentum: the node's impulse and the body's reaction act at one point.
    // (Before the fix the change was about half of drift x reaction.)
    // Free rigid body, free solid, no gravity; the patch starts 0.3 m off its
    // anchors and carries a load from the material's own initial motion.
    {auto body=physics.CreateDynamicBox({0,2,0},{.1f,.1f,.1f},2,0,0);DeformableSettings free;free.material.damping=0;free.material.density=50;free.selfContact=false;
     DeformableAttachment pin;pin.group="left";pin.kind=DeformableAttachment::Kind::Body;pin.target=1;pin.offset={0,-1.7,.2};free.attachments={pin};
     DeformableInstance arm;arm.Initialize(block,free,glm::dmat4(1),30);arm.Impulse("",{0,0,3});
     DeformableTarget target;target.valid=true;target.body=body;const auto pose=physics.GetTransform(body);
     target.previous=target.current=glm::translate(glm::dmat4(1),glm::dvec3(pose.position))*glm::mat4_cast(glm::dquat(pose.rotation));
     double nodeMass=0;for(size_t n=0;n<block->nodes.size();++n)nodeMass+=block->measures[n]*free.material.density;
     const auto angular=[&]{glm::dvec3 l=glm::dvec3(glm::cross(physics.GetCenterOfMass(body),physics.GetLinearVelocity(body)))*double(physics.GetMass(body))+glm::dvec3(physics.GetInertiaWorld(body)*physics.GetAngularVelocity(body));
      for(size_t n=0;n<block->nodes.size();++n)l+=glm::cross(arm.positions[n],arm.velocities[n])*block->measures[n]*free.material.density;return l;};
     const auto before=angular();arm.Step(1./60,physics,zero,{target});const auto after=angular();
     double reaction=glm::length(glm::dvec3(physics.GetLinearVelocity(body)))*physics.GetMass(body);
     std::printf("DRIFTED_PIN angular_before=%.9g,%.9g,%.9g after=%.9g,%.9g,%.9g body_impulse=%.9g\n",before.x,before.y,before.z,after.x,after.y,after.z,reaction);
     check(std::abs(nodeMass-arm.Mass())<1e-9*arm.Mass(),"lumped node masses match the material mass");
     // The material's own projection keeps angular momentum only to second
     // order (about 2e-4 of a spinning block's per step), so the bound is 1% of
     // the couple an anchor-applied reaction makes: |drift| x |reaction|.
     const double couple=glm::length(glm::dvec3(0,.3,.2))*reaction;
     check(reaction>.1&&glm::length(after-before)<.01*couple,"drifted body attachment adds no spurious couple");
     physics.DestroyBody(body);}


    // Independent small specimen loads: compression, bending and a zero-net-force
    // twisting couple. All change physical forces, never rest geometry.
    auto specimen=std::make_shared<DeformableAsset>(*block);
    specimen->groups["upperRight"].clear();specimen->groups["lowerRight"].clear();
    for(unsigned n:specimen->groups.at("right"))specimen->groups[specimen->nodes[n].y>0?"upperRight":"lowerRight"].push_back(n);
    check(PrepareDeformableAsset(*specimen,error),"physical load fixture uses valid unique attachment groups");
    DeformableSettings rubber;rubber.selfContact=false;rubber.material.density=100;rubber.material.shearCompliance=.0005;rubber.material.volumeCompliance=1e-6;rubber.material.damping=3;rubber.attachments={pin};rubber.attachments[0].group="left";
    auto loadSpecimen=[&](int mode){DeformableInstance sample;sample.Initialize(specimen,rubber,glm::dmat4(1),30+mode);
        for(int i=0;i<90;++i){if(mode==0)sample.Force("right",{-20,0,0});if(mode==1)sample.Force("right",{0,0,20});if(mode==2){sample.Force("upperRight",{0,0,10});sample.Force("lowerRight",{0,0,-10});}sample.Step(1./60,physics,zero,{anchor});}
        double loaded=0;for(size_t n=0;n<specimen->nodes.size();++n)loaded=std::max(loaded,glm::length(sample.positions[n]-specimen->nodes[n]));
        check(sample.error.empty()&&sample.stats.minimumJacobian>.85&&loaded>.001,"solid physically deforms while resisting volumetric collapse");
        for(int i=0;i<300;++i){sample.Step(1./60,physics,zero,{anchor});}
        double remaining=0;for(size_t n=0;n<specimen->nodes.size();++n)remaining=std::max(remaining,glm::length(sample.positions[n]-specimen->nodes[n]));
        std::printf("ELASTIC mode=%d loaded=%g unloaded=%g minJ=%g\n",mode,loaded,remaining,sample.stats.minimumJacobian);check(remaining<loaded*.5&&sample.error.empty(),"elastic specimen recovers after physical compression/bend/twist load removal");};
    loadSpecimen(0);loadSpecimen(1);loadSpecimen(2);
    // Mixed cloth/solid collision through the same boundary surface path.
    DeformableInstance cushion,cover;DeformableSettings mixed=thin;mixed.selfContact=false;mixed.material.thickness=.03;
    cushion.Initialize(block,mixed,glm::dmat4(1),40);cover.Initialize(pinSheet,mixed,glm::rotate(glm::translate(glm::dmat4(1),glm::dvec3(0,.515,0)),glm::radians(-90.),glm::dvec3(1,0,0)),41);
    auto cushionBefore=cushion.positions;DeformableInstance::SurfaceContacts(cover,cushion,1./60);check(cover.stats.contacts>0&&cushion.positions!=cushionBefore,"cloth/volumetric surface contact moves both finite-mass participants");
    // Inversion is reported without a rest-pose repair.
    DeformableInstance inverted;inverted.Initialize(block,rubber,glm::dmat4(1),42);for(auto& p:inverted.positions)p.x=-p.x;inverted.Step(1./60,physics,zero,{});check(!inverted.error.empty(),"inverted unsupported solid stops diagnostically");
    // Local simulation coordinates are independent of large absolute placement.
    const glm::dvec3 fixedOrigin(1e12,-2e12,3e12);auto absolute=fixedOrigin+glm::dvec3(1,2,3);DeformableInstance local;local.Initialize(pinSheet,thin,glm::translate(glm::dmat4(1),absolute-fixedOrigin),43);local.Step(1./60,physics,zero,{});check(glm::length(local.positions[0]-(pinSheet->nodes[0]+glm::dvec3(1,2,3)))<1e-12,"fixed-origin large absolute placement preserves precise local simulation");

    // Relative contact friction is measured against an actually moving finite
    // rigid support, not a world-space damping force.
    auto slidingSupport=physics.CreateDynamicBox({0,0,-.35f},{.8f,.5f,.8f},80,0,0);physics.SetLinearVelocity(slidingSupport,{.5f,0,0});
    auto smallSheet=std::make_shared<DeformableAsset>(MakeDeformableSheet(5,5,.7,.7));DeformableSettings friction=thin;friction.selfContact=false;friction.material.friction=.7;friction.material.damping=0;DeformableInstance rider;rider.Initialize(smallSheet,friction,glm::rotate(glm::translate(glm::dmat4(1),glm::dvec3(0,.53,0)),glm::radians(-90.),glm::dvec3(1,0,0)),44);
    for(int i=0;i<45;++i){physics.Step(1.f/60);rider.Step(1./60,physics,down,{});}double meanX=0;for(auto velocity:rider.velocities)meanX+=velocity.x/rider.velocities.size();std::printf("MOVING_CONTACT mean_cloth_vx=%g support_vx=%g\n",meanX,physics.GetLinearVelocity(slidingSupport).x);check(meanX>.05&&meanX<.7&&rider.error.empty(),"friction transfers actual moving support velocity to cloth");physics.DestroyBody(slidingSupport);
    // Bilateral M39 primitive contact filter: the identical block falls through
    // excluded geometry, independent of render visibility.
    auto filteredFloor=physics.CreateStaticBox({0,-.7f,0},{2,.2f,2},0,0);DeformableSettings filtered=cfg;filtered.collisionMask=0;filtered.selfContact=false;DeformableInstance rejected;rejected.Initialize(block,filtered,glm::dmat4(1),45);for(int i=0;i<45;++i)rejected.Step(1./60,physics,down,{});check(rejected.Maximum().y<-.5&&rejected.error.empty(),"collision mask rejects ordinary rigid primitive contact");physics.DestroyBody(filteredFloor);
    // Separate membrane and bend controls against deliberate independent
    // geometric perturbations. No force-driven fixture retunes solver quality.
    auto controls=std::make_shared<DeformableAsset>(MakeDeformableSheet(2,2,1,1));
    auto response=[&](int mode,bool stiff){DeformableSettings settings;settings.selfContact=false;settings.substeps=1;settings.iterations=4;settings.material.damping=0;settings.material.stretchCompliance=settings.material.shearCompliance=settings.material.bendCompliance=1e6;
        if(mode==0)settings.material.stretchCompliance=stiff?0:1e6;
        if(mode==1)settings.material.shearCompliance=stiff?0:1e6;
        if(mode==2)settings.material.bendCompliance=stiff?0:1e6;
        DeformableInstance sample;sample.Initialize(controls,settings,glm::dmat4(1),50+mode);
        if(mode==0)for(auto& p:sample.positions)p.x*=1.2;
        if(mode==1)for(auto& p:sample.positions)p.x+=.2*p.y;
        if(mode==2)sample.positions.back().z=.5;
        auto displaced=sample.positions;sample.Step(1./60,physics,zero,{});double correction=0;for(size_t n=0;n<displaced.size();++n)correction+=glm::length(displaced[n]-sample.positions[n]);return correction;};
    for(int mode=0;mode<3;++mode){double stiff=response(mode,true),soft=response(mode,false);std::printf("MATERIAL_CONTROL mode=%d stiffCorrection=%g softCorrection=%g\n",mode,stiff,soft);check(stiff>.001&&soft<stiff*.01,"independent stretch/shear/bend parameter changes physical response");}
    auto deadBody=physics.CreateDynamicBox({0,1,0},{.2f,.2f,.2f},1,0,0);physics.DestroyBody(deadBody);auto reusedBody=physics.CreateDynamicBox({0,1,0},{.2f,.2f,.2f},1,0,0);
    check(!physics.IsBodyEnabled(deadBody)&&physics.IsBodyEnabled(reusedBody),"destroyed support handle cannot alias reused rigid slot");physics.DestroyBody(reusedBody);
    auto inactiveBefore=follower.positions;follower.settings.enabled=false;check(!follower.Force("",{0,0,2}),"disabled deformable rejects accumulated force submission");follower.Step(1./60,physics,down,{});check(follower.positions==inactiveBefore,"disabled deformation remains frozen independent of presentation");
    auto invalidMesh=*sheet;invalidMesh.render.indices.resize(65538);check(!PrepareDeformableAsset(invalidMesh,error),"bake diagnoses render index payload bound before export");
    auto hugeCloth=*sheet;for(auto& node:hugeCloth.nodes)node*=1e160;
    check(!PrepareDeformableAsset(hugeCloth,error),"finite coordinates with overflowing derived cloth area reject");
    auto hugeSolid=*block;for(auto& node:hugeSolid.nodes)node*=1e110;
    check(!PrepareDeformableAsset(hugeSolid,error),"finite coordinates with overflowing derived tet volume reject");
    auto heavy=std::make_shared<DeformableAsset>(*sheet);heavy->massWeights.assign(heavy->nodes.size(),1e308);
    check(PrepareDeformableAsset(*heavy,error),"finite authored mass weights prepare normally");
    DeformableSettings extreme;extreme.material.density=1e308;bool invalidMassRejected=false;
    try{DeformableInstance invalid;invalid.Initialize(heavy,extreme,glm::dmat4(1),60);}catch(const std::invalid_argument&){invalidMassRejected=true;}
    check(invalidMassRejected,"overflowing derived node mass rejects instance initialization");
    extreme.material.density=1e-308;DeformableInstance finiteMass;finiteMass.Initialize(heavy,extreme,glm::dmat4(1),61);
    auto priorMass=finiteMass.Mass();auto rejectedMaterial=extreme.material;rejectedMaterial.density=1e308;
    check(!finiteMass.SetMaterial(rejectedMaterial,error)&&!error.empty(),"overflowing runtime material is diagnosed");
    check(finiteMass.Mass()==priorMass&&finiteMass.settings.material.density==extreme.material.density,"rejected material change leaves existing mass and configuration unchanged");
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
