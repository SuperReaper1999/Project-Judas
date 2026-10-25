#include "Project.h"
#include "LiquidSurface.h"
#include "LiquidSystem.h"
#include "RuntimeWorld.h"
#include "UniformGravity.h"
#include "RadicalGravity.h"
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
int main(){
 unsigned checks=0,failed=0;
 auto check=[&](bool ok,const char* text){++checks;failed+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);};
 auto near=[](double a,double b,double epsilon=1e-8){return std::abs(a-b)<epsilon;};
 std::string error;GravityEquilibrium eq;UniformGravity({0,-9.81f,0}).Equilibrium({},eq);
 auto geometry=LiquidBox({0,0,0},{8,2,4});LiquidBasinData basin;
 check(BakeLiquidBasin(geometry,eq,1e-6,.002,"core",basin,error),"M54 physical cavity bake");
 LiquidSurfaceSettings settings;settings.enabled=true;settings.columns=16;settings.rows=8;
 std::shared_ptr<const LiquidSurfaceData> data;
 check(BakeLiquidSurface(basin,settings,data,error),"physical surface partition bake");
 if(!data){std::printf("%s\n",error.c_str());return 1;}
 check(data->cells.size()==128&&data->faces.size()==232,"expected cell and shared-face connectivity");
 bool storageExact=true;
 for(auto& c:data->cells)for(unsigned i=1;i<10;++i){double v=c.storage.capacity*i/10.,q=LiquidInverse(c.storage,v);storageExact&=near(LiquidCapacity(c.storage.geometry,eq,q),v,1e-10);}
 check(storageExact,"cached cubic storage/inversion agrees with independent geometric clipping");
 auto disconnected=LiquidBox({0,0,0},{1,.3,1});auto upper=LiquidBox({0,1,0},{1,1.3,1});disconnected.cells.insert(disconnected.cells.end(),upper.cells.begin(),upper.cells.end());
 LiquidBasinData disconnectedBasin;std::shared_ptr<const LiquidSurfaceData> rejected;
 bool disconnectedBake=BakeLiquidBasin(disconnected,eq,1e-5,.002,"disconnected",disconnectedBasin,error);
 check(disconnectedBake&&!BakeLiquidSurface(disconnectedBasin,settings,rejected,error,&disconnected),"coarse cells cannot silently join vertically disconnected pockets");

 // Cached plane/edge topology must still represent independent cap clipping
 // at changing interpolation coordinates, including interval crossings.
 LiquidSurface capProbe(data,18.72);bool capAgreement=true;
 auto area=[](const MeshData& mesh){double sum=0;for(size_t i=0;i+2<mesh.vertices.size();i+=3)sum+=glm::length(glm::cross(glm::dvec3(mesh.vertices[i+1].position-mesh.vertices[i].position),glm::dvec3(mesh.vertices[i+2].position-mesh.vertices[i].position)))*.5;return sum;};
 for(double change:{.64,-10.,7.}){capProbe.BeginStep();capProbe.Apply(capProbe.Plan(change),change>0);
  for(float alpha:{0.f,.3f,.8f,1.f,.3f}){auto actual=capProbe.Mesh(alpha);MeshData reference;
   for(unsigned i=0;i<capProbe.cells.size();++i)for(auto& tet:capProbe.cells[i].storage.geometry.cells){std::array<double,4> coordinates;for(unsigned k=0;k<4;++k)coordinates[k]=eq.Coordinate(tet[k]);LiquidCap(tet,coordinates,capProbe.Coordinate(i,alpha),reference);}
   capAgreement&=!actual.vertices.empty()&&near(area(actual),area(reference),1e-5);
  }
 }
 check(capAgreement,"cached interpolated cap topology agrees with independent geometric clipping");
 LiquidSurface calm(data,32);double initial=calm.Volume();bool rest=true;
 for(unsigned step=0;step<30;++step){rest&=calm.Step(1./60,error);for(auto& cell:calm.cells)rest&=near(cell.q,1,1e-7);}
 check(rest&&near(calm.Volume(),initial),"calm equal-coordinate water remains at rest and conserves volume");
 auto wedge=geometry;for(auto& t:wedge.cells)for(auto& p:t)if(p.y==0)p.y=.12*p.x;
 LiquidBasinData uneven;std::shared_ptr<const LiquidSurfaceData> unevenData;
 bool baked=BakeLiquidBasin(wedge,eq,1e-5,.002,"wedge",uneven,error)&&BakeLiquidSurface(uneven,settings,unevenData,error);
 check(baked,"uneven floor bake retains geometric storage");
 if(baked){LiquidSurface lake(unevenData,24);double q=lake.cells.front().q;bool ok=true;for(unsigned k=0;k<30;++k){ok&=lake.Step(1./60,error);for(auto& c:lake.cells)ok&=near(c.q,q,1e-6);}
 check(ok,"uneven lake at rest has no artificial pressure-driven motion");
 LiquidSurface shoreline(unevenData,1);double wetVolume=shoreline.Volume();bool resting=true;double biggestFlow=0;
 for(unsigned k=0;k<120;++k){resting&=shoreline.Step(1./60,error);for(double f:shoreline.discharge)biggestFlow=std::max(biggestFlow,std::abs(f));}
 check(resting&&near(shoreline.Volume(),wetVolume)&&biggestFlow<1e-7,"partly dry sloping shoreline remains well-balanced");
 unsigned wet=0,dry=0;for(auto& c:shoreline.cells)(c.volume>1e-8?wet:dry)++;
 check(wet>0&&dry>0,"shoreline fixture genuinely contains wet and dry cells");
 shoreline.Impulse({.5,.1,2},{100,0,0},1000);bool wetting=true;double maxEnergy=shoreline.Energy(),start= maxEnergy;
 for(unsigned k=0;k<180;++k){if(!shoreline.Step(1./60,error)){wetting=false;std::printf("shoreline step %u residual %.12g retries %u %s\n",k,shoreline.stats.residual,shoreline.stats.retries,error.c_str());break;}for(auto& c:shoreline.cells)wetting&=c.volume>=0;maxEnergy=std::max(maxEnergy,shoreline.Energy());}
 check(wetting&&near(shoreline.Volume(),wetVolume,1e-7),"wet/dry disturbance stays nonnegative and conservative");
 check(maxEnergy<=start+1e-7,"wet/dry disturbance does not grow uncontrolled energy");
 }
 LiquidSurface wave(data,32);auto debit=wave.Plan(-.08,glm::dvec3(6,1,2));auto credit=wave.Plan(.08,glm::dvec3(1,1,2));
 wave.Apply(debit,false);wave.Apply(credit,true);
 // Spatial exchanges must disturb locally, rather than flatten or move the mean.
 check(!near(wave.cells[wave.Cell({1,1,2})].q,wave.cells[wave.Cell({6,1,2})].q,1e-5),"spatial disturbance is localized");
 double startEnergy=wave.Energy(),maximum=startEnergy;bool solved=true,positive=true,conserved=true;
 for(unsigned step=0;step<360;++step){solved&=wave.Step(1./60,error);if(!solved){std::printf("step %u residual %.12g retries %u error %s\n",step,wave.stats.residual,wave.stats.retries,error.c_str());break;}
  maximum=std::max(maximum,wave.Energy());for(auto& c:wave.cells)positive&=c.volume>=0;conserved&=near(wave.Volume(),32,1e-7);
 }
 check(solved,"disturbance solves within bounded pressure/continuity budget");check(positive&&conserved,"shared-face propagation preserves nonnegative conserved volume");
 check(maximum<=startEnergy+1e-7&&wave.Energy()<startEnergy,"disturbance energy is bounded and damped");
 std::printf("energy initial %.12g peak %.12g final %.12g\n",startEnergy,maximum,wave.Energy());

 auto bakePath=std::filesystem::temp_directory_path()/("judas-surface-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".judasbasin");
 basin.dynamicSurface=data;
 check(SaveLiquidBasin(bakePath.string(),basin,error),"surface geometry bake serializes through normal basin asset");
 LiquidBasinData reloaded;
 check(LoadLiquidBasin(bakePath.string(),reloaded,error)&&reloaded.dynamicSurface&&reloaded.dynamicSurface->faces.size()==data->faces.size(),"baked surface cells/faces round trip");
 std::filesystem::remove(bakePath);
 LiquidSystem system;LiquidBasinSettings authored;authored.initialVolume=1;
 auto source=system.AddBasin(1,authored,{},std::make_shared<LiquidBasinData>(basin),error);authored.initialVolume=0;
 SceneTransform placement;placement.position={20,0,0};auto staticBasin=std::make_shared<LiquidBasinData>(basin);staticBasin->dynamicSurface.reset();auto target=system.AddBasin(2,authored,placement,staticBasin,error);
 check(source.id&&target.id,"dynamic owners use normal world-generation liquid handles");
 system.SurfaceImpulse(source,{1,.01,1},{1,0,0});
 double accepted=system.Transfer(source,target,.8);
 check(near(accepted,.8)&&near(system.Get(source)->volume,.2)&&near(system.Get(source)->dynamicSurface->Volume(),.2),"1000 to 200 litre drain changes partition and owner together");
 auto snapshot=system.Get(source)->dynamicSurface->discharge;
 check(std::any_of(snapshot.begin(),snapshot.end(),[](double v){return v!=0;}),"substantial drain retains nonzero waves rather than resetting the surface");
 accepted=system.Transfer(target,source,.8);
 check(near(accepted,.8)&&near(system.Get(source)->dynamicSurface->Volume(),1)&&system.Get(source)->dynamicSurface->discharge==snapshot,"1000 litre refill preserves moving flow state");
 auto ledger=system.Accounting("water");check(std::abs(ledger.error)<=ledger.tolerance,"surface cells are not counted again in system ledger");
 check(system.SurfaceEnabled(source,false)&&near(system.Get(source)->dynamicSurface->Volume(),1),"surface suspension retains allocation");
 check(system.Remove(source)&&!system.Get(source),"dynamic owner removal uses normal safe lifetime");

 LiquidSurface withdrawal(data,32);withdrawal.Impulse({1,1,2},{100,0,0},1000);
 auto beforeWithdrawal=withdrawal.discharge;auto localDebit=withdrawal.Plan(-.1,glm::dvec3(1,1,2));
 std::vector<double> retained(withdrawal.cells.size(),1);
 for(auto [i,v]:localDebit.changes)retained[i]=(withdrawal.cells[i].volume-v)/withdrawal.cells[i].volume;
 withdrawal.Apply(localDebit,false);bool momentum=true,untouched=false;
 for(size_t k=0;k<beforeWithdrawal.size();++k){auto& f=data->faces[k];unsigned donor=beforeWithdrawal[k]>=0?f.a:f.b;
  momentum&=near(withdrawal.discharge[k],beforeWithdrawal[k]*retained[donor],1e-12);
  if(retained[donor]==1){untouched=true;momentum&=withdrawal.discharge[k]==beforeWithdrawal[k];}
 }
 check(momentum&&untouched,"withdrawal takes donor momentum while unrelated faces remain unchanged");
 LiquidSurfaceAllocation empty;for(unsigned i=0;i<withdrawal.cells.size();++i){empty.changes.push_back({i,withdrawal.cells[i].volume});empty.amount+=withdrawal.cells[i].volume;}
 withdrawal.Apply(empty,false);
 check(withdrawal.Volume()==0&&std::all_of(withdrawal.discharge.begin(),withdrawal.discharge.end(),[](double v){return v==0;})&&withdrawal.Step(1./60,error),"fully drained surface has no orphan flow and solves at rest");
 auto refill=withdrawal.Plan(10);withdrawal.Apply(refill,true);bool initialWet=true;unsigned activeCells=0;
 for(auto& c:withdrawal.cells){initialWet&=near(LiquidCapacity(c.storage.geometry,eq,c.q),c.volume,1e-10)&&c.velocity==glm::dvec3(0);activeCells+=c.volume>0;}
 check(initialWet&&activeCells==1&&near(refill.amount,.5),"newly wet full cell starts at its physical capacity coordinate and zero arrival momentum");
 bool rewetted=true;unsigned maximumIterations=0;
 for(unsigned i=0;i<120;++i){rewetted&=withdrawal.Step(1./60,error);maximumIterations=std::max(maximumIterations,withdrawal.stats.iterations);for(auto& c:withdrawal.cells)rewetted&=c.volume>=0;}
 check(rewetted&&near(withdrawal.Volume(),refill.amount,1e-9),"dry to full-cell refill propagates conservatively within unchanged bounded solver policy");
 std::printf("full-cell rewet peak pressure iterations %u\n",maximumIterations);

 LiquidSurface displacement(data,32);
 std::vector<glm::dvec4> box={{-1,0,0,-2},{1,0,0,2.4},{0,-1,0,-.1},{0,1,0,.5},{0,0,-1,-1},{0,0,1,1.4}};
 auto bodyStart=std::chrono::steady_clock::now();double overflow=displacement.SetSolids({box},error);
 check(overflow==0&&near(displacement.Volume(),32)&&near(displacement.Capacity(),basin.capacity-.064,1e-7),"real submerged solid removes space, never liquid");
 bool bodyOk=true;for(unsigned k=0;k<180;++k)bodyOk&=displacement.Step(1./60,error);
 check(bodyOk&&near(displacement.Volume(),32,1e-7),"body displacement redistributes through pressure/continuity");
 check(displacement.SetSolids({box,box},error)==0&&near(displacement.Capacity(),basin.capacity-.064,1e-7),"overlapping solid union is not subtracted twice");
 check(displacement.SetSolids({},error)==0&&near(displacement.Capacity(),basin.capacity,1e-7)&&near(displacement.Volume(),32,1e-7),"body exit restores space without adding water");
 std::printf("body fixture including 180 steps %.6f ms\n",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-bodyStart).count());
 // Moderate moving-boundary geometry: no quantity is injected on entry or
 // translation, and leaving restores the physical capacity.
 bool moving=true;double originalVolume=displacement.Volume();
 for(unsigned k=0;k<60;++k){auto shifted=box;double dx=.003*k;shifted[0].w-=dx;shifted[1].w+=dx;moving&=displacement.SetSolids({shifted},error)==0&&displacement.Step(1./60,error)&&near(displacement.Volume(),originalVolume,1e-7);}
 check(moving&&displacement.SetSolids({},error)==0&&near(displacement.Capacity(),basin.capacity,1e-7),"moderate body translation and exit retain water and restore space");
 LiquidSystem local;LiquidBasinSettings localSettings;localSettings.initialVolume=32;auto localOwner=local.AddBasin(81,localSettings,{},std::make_shared<LiquidBasinData>(basin),error);localSettings.initialVolume=0;auto holding=local.AddBasin(82,localSettings,placement,staticBasin,error);
 local.Transfer(localOwner,holding,.02,glm::vec3(1,.5,1));local.Transfer(holding,localOwner,.02,{},glm::vec3(6,.5,1));
 auto trough=local.Sample({1,1,1}),crest=local.Sample({6,1,1});
 check(!trough&&crest&&crest->depth>0&&near(local.Accounting("water").total,32),"local crest/trough occupancy follows spatial transactions without flattening");
 auto capMesh=local.Get(localOwner)->dynamicSurface->Mesh(1);bool capMatch=false;for(auto& v:capMesh.vertices)if(glm::length(v.position-crest->surfacePoint)<.8&&std::abs(v.position.y-crest->surfacePoint.y)<1e-6)capMatch=true;
 check(capMatch,"render cap and authoritative local query agree");
 auto parts=LiquidBox({0,0,0},{3,2,4});auto right=LiquidBox({5,0,0},{8,2,4});parts.cells.insert(parts.cells.end(),right.cells.begin(),right.cells.end());LiquidBasinData split;std::shared_ptr<const LiquidSurfaceData> splitData;
 bool splitBake=BakeLiquidBasin(parts,eq,1e-6,.002,"split",split,error)&&BakeLiquidSurface(split,settings,splitData,error,&parts);
 check(splitBake,"separate pools retain missing cells and closed barriers");
 if(splitBake){LiquidSurface pools(splitData,24);auto allocation=pools.Plan(-24,glm::dvec3(1,1,1));check(near(allocation.amount,12,1e-7),"spatial debit cannot cross a disconnected dry gap");
  pools.Apply(allocation,false);auto addition=pools.Plan(3,glm::dvec3(1,1,1));pools.Apply(addition,true);bool separated=true;
  for(unsigned k=0;k<120;++k){separated&=pools.Step(1./60,error);double rightVolume=0;for(unsigned i=0;i<pools.cells.size();++i)if(splitData->cells[i].centre.x>4)rightVolume+=pools.cells[i].volume;separated&=near(rightVolume,12,1e-9);}
  check(separated,"drained and rewetted island cannot exchange water across missing physical faces");
 }
 auto ridge=LiquidBox({0,0,0},{2,2,2});for(auto part:{LiquidBox({2,1.5,0},{4,2,2}),LiquidBox({4,0,0},{6,2,2})})ridge.cells.insert(ridge.cells.end(),part.cells.begin(),part.cells.end());
 LiquidBasinData ridgeBasin;std::shared_ptr<const LiquidSurfaceData> ridgeData;auto ridgeSettings=settings;ridgeSettings.columns=6;ridgeSettings.rows=2;
 bool ridgeBake=BakeLiquidBasin(ridge,eq,1e-6,.002,"ridge",ridgeBasin,error)&&BakeLiquidSurface(ridgeBasin,ridgeSettings,ridgeData,error,&ridge);
 check(ridgeBake,"dry raised sill retains actual physical wet-connectivity geometry");
 if(ridgeBake){LiquidSurface pools(ridgeData,8);int right=pools.Cell({5.5,1,.5});
  auto connectedToRight=[&](){auto reachable=pools.Plan(-100,glm::dvec3(.5,1,.5));return std::any_of(reachable.changes.begin(),reachable.changes.end(),[&](auto change){return change.first==unsigned(right);});};
  bool disconnected=!connectedToRight();
  for(unsigned k=0;k<30;++k)disconnected&=pools.Step(1./60,error);
  check(disconnected&&near(pools.cells[right].q,1,1e-8),"submerged heads below a dry sill neither connect nor exchange water");
  auto add=pools.Plan(4,glm::dvec3(.5,1,.5));pools.Apply(add,true);bool legal=true,connected=false;
  for(unsigned k=0;k<240;++k){double before=0;bool open=false;for(unsigned i=0;i<pools.cells.size();++i)if(ridgeData->cells[i].centre.x>4)before+=pools.cells[i].volume;
   for(size_t k=0;k<ridgeData->faces.size();++k){auto& f=ridgeData->faces[k];if((ridgeData->cells[f.a].centre.x>4)!=(ridgeData->cells[f.b].centre.x>4))open|=LiquidFaceArea(f.polygons,eq,std::max(pools.cells[f.a].q,pools.cells[f.b].q))>1e-12;}
   legal&=pools.Step(1./60,error);double after=0;for(unsigned i=0;i<pools.cells.size();++i){legal&=pools.cells[i].volume>=0;if(ridgeData->cells[i].centre.x>4)after+=pools.cells[i].volume;}
   if(!open)legal&=near(before,after,1e-10);
   connected|=connectedToRight();
  }
  check(legal&&connected&&near(pools.Volume(),12,1e-8),"refill reconnects regions only through wet sill faces, with no negative storage or remote transfer");
 }
 LiquidSurfaceSettings coarseSettings=settings;coarseSettings.columns=8;coarseSettings.rows=4;std::shared_ptr<const LiquidSurfaceData> coarseData;
 bool coarseBake=BakeLiquidSurface(basin,coarseSettings,coarseData,error);
 if(coarseBake){LiquidSurface coarse(coarseData,32);coarse.Impulse({1,1,2},{200,0,0},1000);bool stable=true;for(unsigned k=0;k<180;++k)stable&=coarse.Step(1./60,error);check(stable&&near(coarse.Volume(),32,1e-7),"coarser surface resolution retains conservative stable propagation");}
 else check(false,"coarser surface resolution bake");
 GravityEquilibrium radial;RadicalGravity({0,0,0},9.81).Equilibrium({0,10,0},radial);
 LiquidBasinData radialBasin;LiquidSurfaceSettings radialSettings=settings;radialSettings.columns=4;radialSettings.rows=4;
 std::shared_ptr<const LiquidSurfaceData> radialData;
 bool radialBake=BakeLiquidBasin(LiquidBox({-.5,10,-.5},{.5,11,.5}),radial,1e-4,.02,"radial",radialBasin,error)&&BakeLiquidSurface(radialBasin,radialSettings,radialData,error);
 check(radialBake,"genuine radial storage/conical cells/geodesic face metrics bake");
 if(radialBake){LiquidSurface lake(radialData,.5);double initial=lake.Volume();lake.Impulse({.1,10.2,.1},{1,0,0},1000);bool ok=true;
  for(unsigned k=0;k<120;++k)ok&=lake.Step(1./60,error);
  check(ok&&near(lake.Volume(),initial,1e-7),"radial lake transport conserves volume under actual radial coordinate");
  bool tangential=true;for(unsigned i=0;i<lake.cells.size();++i)tangential&=std::abs(glm::dot(lake.cells[i].velocity,radial.Up(radialData->cells[i].centre)))<1e-7;
  check(tangential,"radial flow velocities are tangent in each cell frame");
 }

 // A bounded splash is detached owned water, not massless decoration.
 Scene splashScene;auto& splashObject=splashScene.CreateObject("reservoir");splashObject.liquidBasin=LiquidBasinSettings{};auto splashEntity=splashObject.id;RuntimeWorld splashWorld;ProjectSettings experimental;experimental.advancedFluidDynamics=true;splashWorld.ConfigureProject(experimental);
 bool splashBuilt=splashWorld.Build(splashScene,nullptr,error);auto splashData=std::make_shared<LiquidSurfaceData>(*data);splashData->settings.splashSpeed=.001;splashData->settings.parcelBudget=2;
 auto splashBasin=std::make_shared<LiquidBasinData>(basin);splashBasin->dynamicSurface=splashData;LiquidBasinSettings splashSettings;splashSettings.initialVolume=32;
 auto splashOwner=splashWorld.Liquids().AddBasin(splashEntity,splashSettings,{},splashBasin,error);splashWorld.Liquids().SurfaceImpulse(splashOwner,{1,.5,2},{100,0,0});splashWorld.Liquids().Update(splashWorld,1./60);
 auto splashLedger=splashWorld.Liquids().Accounting("water");check(splashBuilt&&splashLedger.detached>0&&splashWorld.Liquids().Parcels().size()<=2&&near(splashLedger.total,32,1e-7),"bounded splash debits real donor water and preserves owner/parcel accounting");
 auto parcel=splashWorld.Liquids().Parcels().begin();if(parcel!=splashWorld.Liquids().Parcels().end()){auto parcelId=parcel->first;double amount=parcel->second.volume;double received=splashWorld.Liquids().Receive(parcelId,splashOwner,amount,glm::vec3(6,.5,2));std::printf("splash reception requested %.12g received %.12g total %.12g\n",amount,received,splashWorld.Liquids().Accounting("water").total);check(near(received,amount)&&near(splashWorld.Liquids().Accounting("water").total,32,1e-7),"parcel reception deposits the actual quantity into a local surface allocation");}else check(false,"splash parcel reception fixture");
 splashWorld.Destroy();
 // A bounded, deliberately impossible tolerance is a rollback fixture, not a
 // tolerated production failure. It must not publish a partial retry.
 auto tight=std::make_shared<LiquidSurfaceData>(*data);tight->settings.tolerance=1e-25;tight->settings.iterations=4;
 LiquidSurface rollback(tight,32);rollback.Impulse({1,1,2},{20,0,0},1000);auto rollbackFlows=rollback.discharge;std::vector<double> rollbackVolumes;for(auto& c:rollback.cells)rollbackVolumes.push_back(c.volume);
 bool rejectedStep=!rollback.Step(.1,error);for(unsigned i=0;i<rollback.cells.size();++i)rejectedStep&=rollback.cells[i].volume==rollbackVolumes[i];
 check(rejectedStep&&rollback.discharge==rollbackFlows,"bounded solve failure restores the entire partition and discharge");
 // The closed cavity boundary reflects a propagating local disturbance.
 LiquidSurface pulse(data,32),halfStep(data,32);pulse.Impulse({1,1,2},{200,0,0},1000);halfStep.Impulse({1,1,2},{200,0,0},1000);
 double arrival=0;bool reflected=false,propagated=true;int far=pulse.Cell({7.75,1,2});
 for(unsigned k=0;k<360;++k){propagated&=pulse.Step(1./60,error)&&halfStep.Step(1./120,error)&&halfStep.Step(1./120,error);arrival=std::max(arrival,std::abs(pulse.cells[far].q-1));if(k>100)for(auto& f:data->faces)if(f.b==unsigned(far)&&pulse.discharge[&f-data->faces.data()]< -1e-5)reflected=true;}
 double difference=0;for(unsigned i=0;i<pulse.cells.size();++i)difference=std::max(difference,std::abs(pulse.cells[i].q-halfStep.cells[i].q));
 std::printf("propagation far amplitude %.9g reflection %d half-dt maximum level difference %.9g m\n",arrival,int(reflected),difference);
 check(propagated&&arrival>1e-5&&reflected,"disturbance propagates to far wall and reverses flow on reflection");
 check(difference<.02,"half timestep has a consistent damped surface state");
 // Separate cameras query actual intervals, not a global submerged flag.
 LiquidSystem optical;LiquidBasinSettings opticalSettings;opticalSettings.initialVolume=32;auto opticalOwner=optical.AddBasin(80,opticalSettings,{},std::make_shared<LiquidBasinData>(basin),error);
 auto projection=glm::perspective(glm::radians(45.f),1.f,.1f,10.f);
 auto down=optical.OpticalPaths(glm::lookAt(glm::vec3(4,3,2),glm::vec3(4,0,2),glm::vec3(0,0,-1)),projection,1,1,1);
 auto away=optical.OpticalPaths(glm::lookAt(glm::vec3(4,3,2),glm::vec3(4,5,2),glm::vec3(0,0,1)),projection,1,1,1);
 check(opticalOwner.id&&near(down[0].x,1.9,1e-5)&&near(down[0].y,2.9,1e-5)&&away[0]==glm::vec2(0),"per-camera water entry/exit intervals follow physical boundaries");
 optical.BeginStep();auto* visual=optical.Get(opticalOwner);auto selected=visual->dynamicSurface->Cell({1,.5,1});double previous=visual->dynamicSurface->cells[selected].q;
 visual->dynamicSurface->Apply(visual->dynamicSurface->Plan(-.02,glm::dvec3(1,.5,1)),false);visual->volume-=.02;
 auto beforeSample=optical.Sample({1,.1f,1},{},0),afterSample=optical.Sample({1,.1f,1},{},1);
 check(beforeSample&&afterSample&&near(beforeSample->coordinate,previous,1e-8)&&afterSample->coordinate<previous,"presentation query uses the same previous/current surface state as rendering");
 // Rotate the physical domain and gravity, rather than presenting a Y-up world.
 auto rotation=glm::angleAxis(.8,glm::normalize(glm::dvec3(1,0,1)));auto rotated=geometry;for(auto& t:rotated.cells)for(auto& p:t)p=rotation*p;
 GravityEquilibrium oblique;UniformGravity(glm::vec3(rotation*glm::dvec3(0,-9.81,0))).Equilibrium({},oblique);
 LiquidBasinData tilted;std::shared_ptr<const LiquidSurfaceData> tiltedData;
 bool rotatedBake=BakeLiquidBasin(rotated,oblique,1e-5,.002,"oblique",tilted,error)&&BakeLiquidSurface(tilted,settings,tiltedData,error,&rotated);
 if(!rotatedBake)std::printf("rotated bake error %s\n",error.c_str());
 check(rotatedBake,"arbitrarily rotated uniform geometry/gravity bakes without world-up assumptions");
 if(rotatedBake){LiquidSurface tiltedLake(tiltedData,32);bool calm=true;for(unsigned k=0;k<30;++k)calm&=tiltedLake.Step(1./60,error);for(auto& c:tiltedLake.cells)if(c.volume>0)calm&=near(c.q,1,1e-6);check(calm&&near(tiltedLake.Volume(),32),"oblique lake retains the correct gravity-normal equilibrium");}
 auto flows=wave.discharge;check(!wave.Step(-1,error)&&wave.discharge==flows&&near(wave.Volume(),32),"invalid timestep cannot mutate state");
 for(double depth:{10.,100.}){LiquidBasinData deep;std::shared_ptr<const LiquidSurfaceData> deepData;
  bool ok=BakeLiquidBasin(LiquidBox({0,0,0},{8,depth,4}),eq,1e-6,.002,"depth",deep,error)&&BakeLiquidSurface(deep,settings,deepData,error);
  check(ok,"deep pool fixed surface resolution bake");if(!ok)continue;LiquidSurface surface(deepData,depth*16);
  double restEnergy=surface.Energy();surface.Impulse({1,depth*.5,1},{1000,0,0},1000);double disturbanceEnergy=surface.Energy()-restEnergy;
  auto start=std::chrono::steady_clock::now();unsigned iterations=0;bool finite=true;
  for(unsigned k=0;k<120;++k){finite&=surface.Step(1./60,error);iterations+=surface.stats.iterations;}
  check(finite&&near(surface.Volume(),depth*16,1e-7),"deep-water implicit stepping is bounded and conservative");
  std::printf("depth %.0f disturbance energy %.9g final %.9g\n",depth,disturbanceEnergy,surface.Energy()-restEnergy);
  std::printf("depth %.0f cells %zu faces %zu average step %.6f ms iterations %.2f\n",depth,surface.cells.size(),surface.discharge.size(),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/120,iterations/120.);
 }
 std::printf("M55 core %u checks %u failures\n",checks,failed);return failed?1:0;
}
