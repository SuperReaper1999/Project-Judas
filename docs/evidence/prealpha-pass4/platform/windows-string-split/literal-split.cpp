#include <cstdio>
#include <cstring>
const char* library=R"JS(
const call=(op,...args)=>globalThis.__judas(op,...args);
export class NetworkEntity {
 constructor(id,generation){this.id=String(id);this.generation=generation}
 get valid(){return call('networkValid',this.id,this.generation)}
 get entity(){return entity(call('networkEntry',this.id,this.generation).entity)}
 get owner(){return call('networkEntry',this.id,this.generation).owner}
 set owner(value){call('networkOwner',this.id,this.generation,value)}
 get state(){return call('networkEntry',this.id,this.generation).state}
 set state(value){call('networkState',this.id,this.generation,value)}
 forget(){return call('networkForget',this.id,this.generation)}
}
export const network={
 host:(options={})=>call('networkHost',options),
 join:(options={})=>call('networkJoin',options),
 close:()=>call('networkClose'),
 replicate:(value,options={})=>{if(!(value instanceof Entity))throw new TypeError('Entity required');const id=call('networkReplicate',value.id,options);return new NetworkEntity(id,call('networkInfo').generation)},
 sendInput:values=>call('networkSendInput',values),
 input:peer=>call('networkInput',String(peer)),
 setInterest:(position,radius)=>call('networkInterest',position,radius),
 get mode(){return call('networkInfo').mode},
 get status(){return call('networkInfo').status},
 get error(){return call('networkInfo').error},
 get peerId(){return call('networkInfo').peerId},
 get port(){return call('networkInfo').port},
 get launch(){return call('networkInfo').launch},
 get stats(){return call('networkInfo').stats},
 get peers(){return call('networkPeers').map(p=>({...p,player:p.player?new LocalPlayer(p.player):null}))},
 get entities(){const generation=call('networkInfo').generation;return call('networkEntities').map(e=>new NetworkEntity(e.id,generation))}
};
export const signals={
 subscribe:(name,phase='fixed')=>call('signalSubscribe',name,phase),
 unsubscribe:token=>call('signalUnsubscribe',token),
 emit:(name,payload=null,phase='fixed')=>call('signalEmit',name,payload,phase),
 send:(target,name,payload=null,phase='fixed')=>{if(!(target instanceof Entity))throw new TypeError('signals.send requires an Entity');return call('signalSend',target.id,name,payload,phase)},
 get stats(){return call('signalStats')}
};
export class PlayerInput {
 constructor(id){this.id=String(id)}
 held(name){return call('playerInput',this.id,'held',name)}
 pressed(name){return call('playerInput',this.id,'pressed',name)}
 released(name){return call('playerInput',this.id,'released',name)}
 axis(name){return call('playerInput',this.id,'axis',name)}
 vector(name){return call('playerInput',this.id,'vector',name)}
 stick(side){return call('playerInput',this.id,'stick',side)}
 get touches(){return call('playerInput',this.id,'touches')}
 get pointer(){return call('playerInput',this.id,'pointer')}
}
export class LocalPlayer {
 constructor(id){this.id=String(id)}
 get valid(){return call('playerExists',this.id)}
 get name(){return call('playerInfo',this.id).name}
 get devices(){return call('playerInfo',this.id).devices}
 get input(){return new PlayerInput(this.id)}
 get entity(){return entity(call('playerEntity',this.id))}
 set entity(value){call('playerEntity',this.id,value?.id??null)}
 get viewport(){return call('playerViewport',this.id)}
 set viewport(value){call('playerViewport',this.id,value)}
 get viewError(){return call('playerInfo',this.id).viewError}
 project(point){return call('playerProject',this.id,point)}
 ray(point={x:.5,y:.5}){return call('playerRay',this.id,point)}
 assign(device,options={}){return call('playerAssign',this.id,device,options)}
 unassign(device){return call('playerUnassign',this.id,device)}
 setInputMap(map){return call('playerMap',this.id,map)}
 setView(pose,fov=70,range={}){return call('playerView',this.id,pose,fov,range)}
 clearView(){return call('playerView',this.id,null)}
 leave(){return call('playerRemove',this.id)}
}
export const players={
 create:(name='Player',map=null)=>new LocalPlayer(call('playerCreate',name,map)),
 get:id=>call('playerExists',String(id))?new LocalPlayer(id):null,
 get list(){return call('playerList').map(id=>new LocalPlayer(id))},
 get devices(){return call('playerDevices')}
};

export class Weather {
 constructor(id){this.id=String(id)}
 get valid(){return call('weatherExists',this.id)}
 get enabled(){return call('weatherEnabled',this.id)}
 set enabled(value){call('weatherEnabled',this.id,value)}
 get state(){return call('weatherState',this.id)}
 transition(profile,duration=1){return call('weatherTransition',this.id,profile,duration)}
 strike(position){return call('weatherStrike',this.id,position)}
}
export const weather={sample:point=>call('weatherSample',point),get quality(){return call('weatherQuality')},set quality(value){call('weatherQuality',value)}};
export class Entity {
 constructor(id){this.id=String(id)}
 get valid(){return call('valid',this.id)}
 get transform(){return call('transform',this.id)}
 get presentedTransform(){return call('presentedTransform',this.id)}
 set transform(value){call('setTransform',this.id,value)}
 get parent(){return entity(call('parent',this.id))}
 get children(){return call('children',this.id).map(entity)}
 setColliderEnabled(enabled){return call('colliderEnabled',this.id,enabled)}
 destroy(){return call('destroy',this.id)}
 hasTag(tag){return call('hasTag',this.id,tag)}
 addTag(tag){return call('addTag',this.id,tag)}
 removeTag(tag){return call('removeTag',this.id,tag)}
 get collider(){return call('colliderInfo',this.id)}
 get sleeping(){return call('sleeping',this.id)}
 get physicalMaterial(){return call('physicalMaterialInfo',this.id)}
 setPhysicalMaterial(asset=null,parameters={}){return call('physicalMaterialSet',this.id,asset,parameters)}
 setSocket(target,joint,offset={}){return target==null?call('socketClear',this.id):call('socketSet',this.id,target.id,joint,offset)}
 clearSocket(){return call('socketClear',this.id)}
 get classification(){return call('classification',this.id)}
 get animation(){return call('animationExists',this.id)?new Animation(this.id):null}
 get liquid(){const h=call("liquidOwner",this.id);return h?new LiquidVolume(h):null}
 get navigationObstacle(){return call("navObstacleInfo",this.id)}
 get navigationLink(){return call("navLinkInfo",this.id)}
 setNavigationEnabled(component,enabled){return call("navEnabled",this.id,component,enabled)}
 get navigation(){return call("navAgentExists",this.id)?new NavigationAgent(this.id):null}
 get fracture(){const epoch=call("fractureExists",this.id);return epoch?new Fracture(this.id,epoch):null}
 get deformable(){const epoch=call('deformableExists',this.id);return epoch?new Deformable(this.id,epoch):null}
 get gravity(){return new EntityGravity(this.id)}
 get character(){return call("characterExists",this.id)?new Character(this.id):null}
 get wheel(){return call('wheelExists',this.id)?new Wheel(this.id):null}
 get powertrain(){return call('powertrainExists',this.id)?new Powertrain(this.id):null}
 get ragdoll(){return call('ragdollExists',this.id)?new Ragdoll(this.id):null}
 get light(){return call("lightInfo",this.id)}
 setLight(settings){return call("lightSettings",this.id,settings)}
 get audio(){return call('audioInfo',this.id)}
 setAudio(settings){return call('audioSettings',this.id,settings)}
 seekAudio(seconds){return call('audioSeek',this.id,seconds)}
 setAudioVelocity(velocity=null){return call('audioVelocity',this.id,velocity)}
 playAudioOneShot(){return call('audioOneShot',this.id)}
 setAudioEnabled(enabled){return call('audioEnabled',this.id,enabled)}
 get terrain(){return new Terrain(this.id)}
 get weather(){return call('weatherExists',this.id)?new Weather(this.id):null}
 get planetState(){return call("planetInfo",this.id)}
 planetSample(absolute){return call("planetSample",this.id,absolute)}
 preparePlanetCollision(absolute){return call("planetCollision",this.id,absolute)}
 clearPlanetCollision(){return call("planetClear",this.id)}
 get camera(){return call('cameraInfo',this.id)}
 get renderVisible(){return call('renderVisible',this.id)}
 set renderVisible(value){call('renderVisible',this.id,value)}
 get rendererVisible(){return call('rendererVisible',this.id)}
 set rendererVisible(value){call('rendererVisible',this.id,value)}
 material(slot=0){return new Material(this.id,slot)}
 get modelParts(){return call('modelParts',this.id)}
 setPartVisible(identity,visible){return call('modelPartVisible',this.id,identity,visible)}
 scriptState(slot){return call('scriptState',this.id,slot)}
 applyForce(value){call('force',this.id,value)}
 applyImpulse(value){call('impulse',this.id,value)}
 applyImpulseAtPoint(impulse,point){call('impulseAtPoint',this.id,impulse,point)}
 applyTorque(value){call('torque',this.id,value)}
 get mass(){return call('mass',this.id)}
 get inertiaWorld(){return call('inertiaWorld',this.id)}
 get velocity(){return call('velocity',this.id)}
 set velocity(value){call('setVelocity',this.id,value)}
 get angularVelocity(){return call('angularVelocity',this.id)}
 set angularVelocity(value){call('setAngularVelocity',this.id,value)}
 get motionType(){return call('motionType',this.id)}
 setMotionType(type,options={}){return call('motionTypeSet',this.id,type,options)}
 moveKinematic(target,seconds=0){return call('kinematicTarget',this.id,target,seconds)}
 setKinematicVelocity(linear,angular={x:0,y:0,z:0}){return call('kinematicVelocity',this.id,linear,angular)}
 stopKinematic(){return call('kinematicStop',this.id)}
 get kinematicMotion(){return call('kinematicMotion',this.id)}
 pointVelocity(point){return call('pointVelocity',this.id,point)}
 playAudio(){return call('playAudio',this.id)}
 stopAudio(){return call('stopAudio',this.id)}
 pauseAudio(){return call('pauseAudio',this.id)}
 resumeAudio(){return call('resumeAudio',this.id)}
 burst(count){return call('burst',this.id,count)}
 setParticles(settings){return call('particles',this.id,settings)}
 setCameraEnabled(enabled){return call('camera',this.id,enabled)}
 setCameraProjection(range){return call('cameraProjection',this.id,range)}
}
export const audio={group:name=>call('audioGroup',name),setGroup:(name,settings,fadeSeconds=0)=>call('audioGroupSet',name,settings,fadeSeconds),get diagnostics(){return call('audioDiagnostics')}};
export const entity=id=>id&&id!=='0'?new Entity(id):null;
export const world={entity,get origin(){return call("worldOrigin")},get originState(){return call("worldOriginState")},toAbsolute:point=>call("worldAbsolute",point),toLocal:point=>call("worldLocal",point),requestOrigin:point=>call("worldOriginRequest",point),get appearance(){return call('appearanceInfo')},setAppearance:settings=>call('appearanceSet',settings),resetAppearance:()=>call('appearanceReset'),setView:(pose,fov=70,range={})=>call("setView",pose,fov,range),project:point=>call("projectViewport",point),get viewport(){return call("viewportSize")},clearView:()=>call("clearView"),fluidSample:(point,up,halfHeight,radius,tangent)=>call("fluidSample",point,up,halfHeight,radius,tangent),get viewRay(){return call('viewRay')},queryTags:(required=[],excluded=[])=>call('queryTags',required,excluded).map(entity),
 spawnPrefab:(asset,transform={},options={})=>entity(call('spawn',asset,transform,options)),
 overlap:(min,max,filter={})=>call('overlap',min,max,filter).map(entity),
 sweepCapsule:(from,displacement,rotation={w:1,x:0,y:0,z:0},filter={})=>call('sweep',from,displacement,filter,rotation)};
const castResult=hit=>hit?{...hit,entity:entity(hit.entityId)}:null;
const cast=(origin,direction,maximum,filter,shape)=>castResult(call('cast',origin,direction,filter,{...shape,maximum}));
const castMany=(kind,requests,filter)=>call('castMany',kind,requests,filter).map(castResult);
export class Material {
 constructor(entityId,slot=0){this.entityId=entityId;this.slot=slot}
 get state(){return call('materialInfo',this.entityId,this.slot)}
 assign(asset){return call('materialAssign',this.entityId,this.slot,asset)}
 set(parameters){return call('materialOverride',this.entityId,this.slot,parameters)}
 clearOverrides(){return call('materialClear',this.entityId,this.slot)}
}
export class Fracture {
 constructor(id,epoch){this.id=String(id);this.epoch=String(epoch)}
 get valid(){return call('fractureValid',this.id,this.epoch)}
 get state(){const state=call('fractureState',this.id,this.epoch);state.parts=state.parts.map(p=>({...p,entity:p.entity?new Entity(p.entity):null}));return state}
 release(interfaceKey,revision){return call('fractureRelease',this.id,this.epoch,interfaceKey,revision)}
 remove(part,revision){return call('fractureRemove',this.id,this.epoch,part,revision)}
 force(part,value){return call('fractureForce',this.id,this.epoch,part,value)}
 impulse(part,value){return call('fractureImpulse',this.id,this.epoch,part,value)}
}
export class Deformable {
 constructor(id,epoch){this.id=id;this.epoch=epoch}
 get valid(){return call('deformableValid',this.id,this.epoch)}
 get state(){return call('deformableState',this.id,this.epoch)}
 get enabled(){return this.state.enabled}
 set enabled(value){call('deformableEnabled',this.id,this.epoch,value)}
 reset(){return call('deformableReset',this.id,this.epoch)}
 force(value,group=''){return call('deformableForce',this.id,this.epoch,value,group)}
 impulse(value,group=''){return call('deformableImpulse',this.id,this.epoch,value,group)}
 impulseAt(location,value){return call('deformableHitImpulse',this.id,this.epoch,location,value)}
 release(group){return call('deformableRelease',this.id,this.epoch,group)}
 attach(group,options){return call('deformableAttach',this.id,this.epoch,group,{...options,targetId:options.target?.id})}
 setMaterial(options){return call('deformableMaterial',this.id,this.epoch,options)}
 raycast(origin,direction,maximum){return call('deformableRaycast',this.id,this.epoch,origin,direction,maximum)}
}
)JS" R"JS(export class EntityGravity {
 constructor(id){this.id=id}
 get state(){const v=call('entityGravityState',this.id);return {...v,source:entity(v.sourceId)}}
 get acceleration(){return this.state.acceleration}
 select(source){return call('entityGravitySelect',this.id,source?.id??'0')}
 setUniform(acceleration){return call('entityGravityUniform',this.id,acceleration)}
 clear(){return call('entityGravityClear',this.id)}
}
export class Terrain {
 constructor(id){this.id=id}
 get state(){return call('terrainState',this.id)}
 create(options){return call('terrainCreate',this.id,options)}
 brush(point,options){return call('terrainBrush',this.id,point,options)}
}
export class Character {
 constructor(id){this.id=id}
 get state(){const v=call('characterState',this.id);return {...v,supportEntity:entity(v.supportEntityId)}}
 get velocity(){return this.state.velocity}
 set velocity(v){call('characterVelocity',this.id,v)}
 get supported(){return this.state.supported}
 get supportNormal(){return this.state.supportNormal}
 get supportVelocity(){return this.state.supportVelocity}
 get actualDisplacement(){return this.state.actualDisplacement}
 get gravity(){return this.state.gravity}
 get up(){return this.state.up}
 set enabled(v){call('characterEnabled',this.id,v)}
 configure(v){return call('characterConfigure',this.id,v)}
 accelerate(v){return call('characterAcceleration',this.id,v)}
 displace(v){return call('characterDisplacement',this.id,v)}
 orient(q){return call('characterOrientation',this.id,q)}
 ignore(entities){return call('characterIgnore',this.id,entities.map(e=>e.id))}
}
export class Wheel {
 constructor(id){this.id=String(id)}
 get valid(){return call('wheelValid',this.id)}
 get enabled(){return this.settings.enabled}
 set enabled(value){call('wheelEnabled',this.id,value)}
 get settings(){const v=call('wheelSettings',this.id);return {...v,chassis:entity(v.chassisId)}}
 get state(){const v=call('wheelState',this.id);return {...v,contactEntity:entity(v.contactEntityId)}}
 set(commands){return call('wheelSet',this.id,commands)}
 configure(settings){return call('wheelConfigure',this.id,settings)}
}
export class Powertrain {
 constructor(id){this.id=String(id)}
 get valid(){return call('powertrainValid',this.id)}
 get enabled(){return this.settings.enabled}
 set enabled(value){call('powertrainEnabled',this.id,value)}
 get settings(){return call('powertrainSettings',this.id)}
 get state(){return call('powertrainState',this.id)}
 set(commands){return call('powertrainSet',this.id,commands)}
}
export class LiquidVolume {
 constructor(handle){this.handle=handle}
 get valid(){return call('liquidValid',this.handle)}
 get state(){const v=call('liquidState',this.handle);return {...v,entity:entity(v.entityId)}}
 set enabled(value){call('liquidEnabled',this.handle,value)}
 transferTo(destination,volume,options={}){return call('liquidTransfer',this.handle,destination.handle,volume,options)}
 applyImpulse(point,impulse){return call('liquidImpulse',this.handle,point,impulse)}
 set surfaceEnabled(value){call('liquidSurfaceEnabled',this.handle,value)}
}
)JS" R"JS(export const liquid={
 sample:point=>{const v=call('liquidSample',point);return v?{...v,entity:entity(v.entityId)}:null},
 samplePresented:point=>{const v=call('liquidPresentedSample',point);return v?{...v,entity:entity(v.entityId)}:null},
 accounting:(material='water')=>call('liquidAccounting',material),
 get errors(){return call('liquidErrors')},
 get connections(){return call('liquidConnections')},
 submerged:target=>call('liquidSubmerged',target.id)
};
export class NavigationAgent {
 constructor(id){this.id=id}
 get state(){const s=call('navAgentState',this.id);return {...s,link:entity(s.linkId)}}
 set enabled(value){call("navEnabled",this.id,"agent",value)}
 setDestination(point){return call('navDestination',this.id,point)}
 clear(){return call('navClear',this.id)}
 set stopped(value){call('navStopped',this.id,value)}
 get stopped(){return this.state.stopped}
 get steering(){return this.state.steering}
 get remainingDistance(){return this.state.remainingDistance}
 completeLink(){return call('navCompleteLink',this.id)}
 configure(settings){return call('navConfigure',this.id,settings)}
}
const navResult=r=>r?{...r,surface:entity(r.surfaceId)}:null;
const navPath=p=>({...p,corners:p.corners.map(c=>({...c,link:entity(c.linkId)}))});
export const navigation={
 sample:(point,range=2,filter={})=>navResult(call('navSample',point,range,filter)),
 path:(start,end,filter={})=>navPath(call('navPath',start,end,filter)),
 raycast:(start,end,filter={})=>navResult(call('navRaycast',start,end,filter)),
 get areas(){return call('navAreas')},get profiles(){return call('navProfiles')},get errors(){return call('navErrors')}
};
export class Ragdoll {
 constructor(id){this.id=id}
 get active(){return call('ragdollActive',this.id)}
 enter(){return call('ragdollEnter',this.id)}
 leave(seconds=.4){return call('ragdollLeave',this.id,seconds)}
 set enabled(value){call('ragdollEnabled',this.id,value)}
 get receiveContactEvents(){return call('ragdollContactEvents',this.id)}
 set receiveContactEvents(value){call('ragdollContactEvents',this.id,value)}
 body(joint){return entity(call('ragdollBody',this.id,joint))}
 configurePhysical(settings){return call('ragdollPhysicalConfigure',this.id,settings)}
 setMode(mode,options={}){return call('ragdollMode',this.id,mode,options)}
 get physicalState(){return call('ragdollPhysicalState',this.id)}
 get balanceDiagnostics(){return call('ragdollBalanceDiagnostics',this.id)}
 set balanceDiagnostics(enabled){call('ragdollBalanceDiagnostics',this.id,enabled)}
 configureBalance(settings){return call('ragdollBalanceConfigure',this.id,settings)}
 requestBalance(task='stand',options={}){return call('ragdollBalanceRequest',this.id,task,options)}
 cancelBalance(){return call('ragdollBalanceCancel',this.id)}
 get balanceState(){const r=call('ragdollBalanceState',this.id);return {...r,supports:r.supports.map(s=>({...s,entity:entity(s.entityId)}))}}
}
export class Animation {
 constructor(entityId){this.entityId=entityId}
 get info(){return call('animationInfo',this.entityId)}
 get clips(){return this.info.clips}
 get playing(){return this.info.playing}
 get time(){return this.info.time}
 get speed(){return this.info.speed}
 set speed(speed){call('animationSet',this.entityId,{speed})}
 get loop(){return this.info.loop}
 set loop(loop){call('animationSet',this.entityId,{loop})}
 play(clip=''){return call('animationPlay',this.entityId,clip)}
 pause(){return call('animationPause',this.entityId)}
 resume(){return this.play()}
 stop(){return call('animationStop',this.entityId)}
 seek(time){return call('animationSeek',this.entityId,time)}
 advance(seconds){return call('animationAdvance',this.entityId,seconds)}
 get sample(){return call('animationSample',this.entityId)}
 rootMotion(clip,from,to,loop=false){return call('animationRootMotion',this.entityId,clip,from,to,loop)}
 crossFade(clip,seconds=.3){return call('animationFade',this.entityId,clip,seconds)}
 get layers(){return this.info.layers}
 layer(id,settings){return call('animationLayer',this.entityId,id,settings)}
 removeLayer(id){return call('animationRemoveLayer',this.entityId,id)}
 jointTransform(key,space='world',presented=false){return call('animationJointPose',this.entityId,key,space,presented)}
 limb(id,settings){return call('animationLimb',this.entityId,id,settings)}
 removeLimb(id){return call('animationRemoveLimb',this.entityId,id)}
 configureIK(settings){return call('animationIKConfigure',this.entityId,settings)}
 ikTargets(targets){return call('animationIKTargets',this.entityId,targets)}
 get ikStatus(){return call('animationIKStatus',this.entityId)}
}
export class Joint {
 constructor(id){this.id=String(id)}
 get valid(){return call('jointValid',this.id)}
 get state(){return call('jointState',this.id)}
 configure(settings){return call('jointConfigure',this.id,settings)}
 destroy(){return call('jointDestroy',this.id)}
 setEnabled(enabled){return call('jointSet',this.id,{enabled})}
 setLimits(lower,upper,limits=true){return call('jointSet',this.id,{lower,upper,limits})}
 setMotor(speed,maxForce,motor=true){return call('jointSet',this.id,{speed,maxForce,motor})}
 setSpring(rest,stiffness,damping,spring=true){return call('jointSet',this.id,{rest,stiffness,damping,spring})}
}
export const physics={
 gravity:point=>call('gravitySample',point),
 createJoint:(owner,settings)=>new Joint(call('jointCreate',owner.id,{...settings,bodyAId:settings.bodyA?.id,bodyBId:settings.bodyB?.id??'0'})),
 closestPoint:(point,maximum,filter={})=>{const hit=call('closestPoint',point,{x:0,y:0,z:0},filter,{maximum});return hit?{...hit,entity:entity(hit.entityId)}:null},
 joint:owner=>{const id=call('joint',owner.id);return id?new Joint(id):null},
 raycast:(origin,direction,maximum,filter={})=>cast(origin,direction,maximum,filter,{kind:'ray'}),
 raycastMany:(rays,filter={})=>castMany('ray',rays,filter),
 sphereCast:(origin,radius,direction,maximum,filter={})=>cast(origin,direction,maximum,filter,{kind:'sphere',radius}),
 sphereCastMany:(casts,filter={})=>castMany('sphere',casts,filter),
 capsuleCast:(pose,radius,halfHeight,direction,maximum,filter={})=>cast(pose.position,direction,maximum,filter,{kind:'capsule',rotation:pose.rotation,radius,halfHeight}),
 capsuleCastMany:(casts,filter={})=>castMany('capsule',casts,filter),
 boxCast:(pose,halfExtents,direction,maximum,filter={})=>cast(pose.position,direction,maximum,filter,{kind:'box',rotation:pose.rotation,halfExtents})};
export const scenes={get current(){return call('sceneCurrent')},get registered(){return call('sceneList')},load:name=>call('sceneLoad',name),reload:()=>call('sceneReload'),
 get regions(){return call('regionList')},get streamingStats(){return call('regionStats')},
 requestRegion:(name,options={})=>call('regionRequest',name,options),regionStatus:token=>call('regionStatus',token),activateRegion:token=>call('regionActivate',token),releaseRegion:token=>call('regionRelease',token),unloadRegion:name=>call('regionUnload',name),
 owner:entity=>call('regionOwner',entity.id),resolveRegionEntity:(name,local)=>{const id=call('regionResolve',name,String(local));return id?new Entity(id):null},
 pinRegion:(name,reason,pin=true)=>call('regionPin',name,reason,pin),adopt:(entity,name='root')=>call('regionAdopt',entity.id,name),
 setInterest:(name,position,options)=>call('regionInterest',name,position,options),removeInterest:name=>call('regionRemoveInterest',name)};
export const localization={get locale(){return call('localeInfo').locale},get available(){return call('localeInfo').available},get revision(){return call('localeInfo').revision},get direction(){return call('localeInfo').direction},setLocale:locale=>call('localeChoose',locale),format:(key,args={})=>call('localeFormat',key,args),number:(value,options={})=>call('localeNumber',value,options),reload:()=>call('localeReload')};
export const saves={save:(slot,options={})=>call('saveRequest','save',slot,options),load:(slot,options={})=>call('saveRequest','load',slot,options),delete:slot=>call('saveRequest','delete',slot,{}),refresh:()=>call('saveRequest','list','',{}),list:()=>call('saveList'),exists:slot=>call('saveList').some(s=>s.id===slot),status:request=>call('saveStatus',request),cancel:request=>call('saveCancel',request),exclude:(entity,excluded=true)=>call('saveExclude',entity.id,excluded),reference:entity=>call('saveReference',entity.id),resolve:key=>entity(call('saveResolve',key))};
export const session={get:key=>call('sessionGet',key),set:(key,value)=>call('sessionSet',key,value),delete:key=>call('sessionDelete',key)};
export const input={get touches(){return call('touches')},get pointer(){return call('pointer')},get pointerCapture(){return call('pointerCapture')},set pointerCapture(value){call('setPointerCapture',value)},held:name=>call('held',name),pressed:name=>call('pressed',name),released:name=>call('released',name),axis:name=>call('axis',name),stick:side=>call('stick',side),stickDelta:side=>call('stickDelta',side),vector:name=>call('vector',name),stickSamples:(side,after=0)=>call('stickSamples',side,after)};
export const profiler={scope:(label,callback)=>call("profileScope",label,callback),counter:(label,value,mode="sum")=>call("profileCounter",label,value,mode)};
export const mods={get active(){return call('modCatalog').active},contributions:point=>call('modCatalog').contributions.filter(c=>c.point===point),resolve:(owner,name)=>call('modCatalog').exports[owner]?.[name]??null};
export const time={get elapsed(){return call('elapsed')},get delta(){return call('delta')},get fixed(){return call('fixed')}};
export const console={log:(...args)=>call('log',args.map(String).join(' '))};

export class UIElement {
 constructor(handle,id){this.handle=handle;this.id=id}
 get scroll(){return call('uiControl',this.handle,this.id,'scroll')}
 set scroll(value){call('uiControl',this.handle,this.id,'scroll',JSON.stringify(value))}
 get selection(){return call('uiControl',this.handle,this.id,'selection')}
 set selection(value){call('uiControl',this.handle,this.id,'selection',JSON.stringify(value))}
 get selectedIndex(){return call('uiControl',this.handle,this.id,'selected')}
 set selectedIndex(value){call('uiControl',this.handle,this.id,'selected',JSON.stringify(value))}
 setItems(items){call('uiControl',this.handle,this.id,'items',JSON.stringify(items))}
 configure(options){call('uiControl',this.handle,this.id,'configure',JSON.stringify(options))}
 get layout(){return call('uiLayout',this.handle,this.id)}
 setLayout(patch){return call('uiLayout',this.handle,this.id,patch)}
 get text(){return call('uiGet',this.handle,this.id,'text')}
 set text(v){call('uiSet',this.handle,this.id,'text',v)}
 get visible(){return call('uiGet',this.handle,this.id,'visible')}
 set visible(v){call('uiSet',this.handle,this.id,'visible',v)}
 get enabled(){return call('uiGet',this.handle,this.id,'enabled')}
 set enabled(v){call('uiSet',this.handle,this.id,'enabled',v)}
 get value(){return call('uiGet',this.handle,this.id,'value')}
 set value(v){call('uiSet',this.handle,this.id,'value',v)}
 get font(){return call('uiGet',this.handle,this.id,'font')}
 set font(v){call('uiSet',this.handle,this.id,'font',v)}
 get textKey(){return call('uiGet',this.handle,this.id,'textKey')}
 set textKey(v){call('uiSet',this.handle,this.id,'textKey',v)}
 get direction(){return call('uiGet',this.handle,this.id,'direction')}
 set direction(v){call('uiSet',this.handle,this.id,'direction',v)}
 get textAlignment(){return call('uiGet',this.handle,this.id,'textAlignment')}
 set textAlignment(v){call('uiSet',this.handle,this.id,'textAlignment',v)}
 get texture(){return call('uiGet',this.handle,this.id,'texture')}
 set texture(v){call('uiSet',this.handle,this.id,'texture',v)}
}
export class UIDocument {
 constructor(handle){this.handle=handle}
 get referenceSize(){return call('uiControl',this.handle,'','reference')}
 set referenceSize(value){call('uiControl',this.handle,'','reference',JSON.stringify(value))}
 get focused(){return call('uiControl',this.handle,'','focus')}
 focus(id){call('uiControl',this.handle,'','focus',JSON.stringify(id??''))}
 get scale(){return call('uiControl',this.handle,'','scale')}
 set scale(value){call('uiControl',this.handle,'','scale',JSON.stringify(value))}
 captureState(){return call('uiControl',this.handle,'','capture')}
 restoreState(state){call('uiControl',this.handle,'','restore',JSON.stringify(state))}
 get accessibility(){return call('uiControl',this.handle,'','accessibility')}

 get player(){const id=call("uiPlayer",this.handle);return id?new LocalPlayer(id):null}
 set player(value){call("uiPlayer",this.handle,value?.id??null)}
 get(id){call('uiGet',this.handle,id,'visible');return new UIElement(this.handle,id)}
 get visible(){return call('uiGet',this.handle,'','visible')}
 set visible(v){call('uiSet',this.handle,'','visible',v)}
 get enabled(){return call('uiGet',this.handle,'','enabled')}
 set enabled(v){call('uiSet',this.handle,'','enabled',v)}
 get modal(){return call('uiGet',this.handle,'','modal')}
 set modal(v){call('uiSet',this.handle,'','modal',v)}
 show(){this.visible=true} hide(){this.visible=false}
 unload(){call('uiUnload',this.handle)}
}
export const ui={get:name=>{const id=call('uiFind',name);return id?new UIDocument(id):null},
 load:(asset,name)=>new UIDocument(call('uiLoad',asset,name)),quit:()=>call('uiQuit'),
 get debugOverlayVisible(){return call('uiDiagnostics')},set debugOverlayVisible(v){call('uiDiagnostics',v)}};

globalThis.console=console;
)JS";
int main(int argc,char** argv){if(argc!=2)return 2;FILE* f=nullptr;fopen_s(&f,argv[1],"wb");if(!f)return 3;auto n=std::strlen(library);auto written=std::fwrite(library,1,n,f);std::fclose(f);return written==n?0:4;}
