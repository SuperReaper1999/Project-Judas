// A creature's surface pursuit is game behaviour, using exactly the same public
// gravity selection, authoritative geometry and CharacterMotor as the boots.
// No transform is teleported, no hidden attachment is created, and selecting a
// new gravity reference never assigns velocity. The caller applies launch intent.
import {physics} from 'judas';
import {add,sub,mul,dot,length,norm,tangent,rotate,axis,qm} from './math.js';

const cross=(a,b)=>({x:a.y*b.z-a.z*b.y,y:a.z*b.x-a.x*b.z,z:a.x*b.y-a.y*b.x});
const inverse=q=>({w:q.w,x:-q.x,y:-q.y,z:-q.z});
const clamp=v=>Math.max(-1,Math.min(1,v));
const zero=()=>({x:0,y:0,z:0});
function align(a,b){
 const cosine=clamp(dot(a,b));if(cosine>.99999)return {w:1,x:0,y:0,z:0};
 let turn=cross(a,b);
 if(length(turn)<1e-5)turn=cross(a,Math.abs(a.x)<.8?{x:1,y:0,z:0}:{x:0,y:0,z:1});
 return axis(norm(turn),Math.acos(cosine));
}

export default class ZombieFramewalk {
 constructor(entity,seed=0){
  this.entity=entity;this.seed=seed>>>0;this.active=false;this.overridden=false;
  this.mode='ordinary';this.reference=null;this.pending=null;this.age=0;
  this.scan=.12+(this.seed%17)*.013;this.cooldown=0;this.lost=0;this.air=0;
  this.pendingProbe=0;
  this.jumps=0;this.switches=0;this.prior=null;this.targetEntity=null;
  // These match this project's authored zombie capsule. They are content, not
  // another character solver. Every clearance query uses the real same shape.
  this.radius=.3;this.halfHeight=.6;this.offset=.9;this.reach=4.8;
 }

 filter(){return {ignored:[this.entity,...(this.targetEntity?.valid?[this.targetEntity]:[])],
  excludedTags:['enemy'],includeSensors:false};}
 center(){const t=this.entity.transform;return add(t.position,rotate(t.rotation,{x:0,y:this.offset,z:0}));}
 rotationFor(up){const q=this.entity.transform.rotation;
  return qm(align(norm(rotate(q,{x:0,y:1,z:0})),norm(up)),q);}
 ordinaryAcceleration(){
  // A pre-existing uniform override belongs to its original owner. Otherwise
  // this game's ordinary return is the actual spatial field, not a world axis.
  return this.prior?.mode==='uniform'?this.prior.acceleration:physics.gravity(this.center());
 }
 ordinaryUp(){const g=this.ordinaryAcceleration();return length(g)>.01?norm(mul(g,-1)):this.entity.character.up;}
 faceValid(hit){return !!(hit?.entity?.valid&&hit.normal&&
  ['static','kinematic'].includes(hit.entity.motionType)&&length(hit.normal)>.9);}

 referenceFrom(hit){const t=hit.entity.transform;return {entity:hit.entity,
  point:hit.point,normal:norm(hit.normal),
  localPoint:rotate(inverse(t.rotation),sub(hit.point,t.position)),
  localNormal:rotate(inverse(t.rotation),norm(hit.normal))};}
 refresh(reference){if(!reference?.entity?.valid||reference.entity.collider?.enabled===false)return false;
  const t=reference.entity.transform;
  reference.point=add(t.position,rotate(t.rotation,reference.localPoint));
  reference.normal=norm(rotate(t.rotation,reference.localNormal));return true;}

 nearbyFace(reference){
  if(!this.refresh(reference))return null;
  const center=this.center(),n=reference.normal;
  const separation=dot(sub(center,reference.point),n);
  if(separation<.03||separation>this.reach)return null;
  // A remembered remote face is only a search direction. This cast must find a
  // real nearby face on the same body; it cannot teleport to a distant player.
  const hit=physics.raycast(center,mul(n,-1),Math.min(this.reach,separation+.12),this.filter());
  return this.faceValid(hit)&&hit.entity.id===reference.entity.id&&dot(hit.normal,n)>.9?hit:null;
 }

 suitable(hit){
  if(!this.faceValid(hit))return false;
  const center=this.center(),n=norm(hit.normal),distance=length(sub(hit.point,center));
  if(distance>this.reach||dot(n,sub(center,hit.point))<.025)return false;
  const landing=add(hit.point,mul(n,this.halfHeight+this.radius+.065));
  if(physics.capsuleCast({position:landing,rotation:this.rotationFor(n)},
     this.radius,this.halfHeight,n,0,this.filter()))return false;
  const approach=sub(landing,center),travel=length(approach);
  if(travel>.04){
   const obstruction=physics.capsuleCast({position:center,rotation:this.entity.transform.rotation},
    this.radius,this.halfHeight,norm(approach),travel,this.filter());
   if(obstruction&&obstruction.distance<travel-.035&&
      (!obstruction.initialOverlap||dot(norm(approach),obstruction.normal)<.05))return false;
  }
  return true;
 }

 candidate(goal,targetUp){
  const m=this.entity.character,center=this.center(),u=m.up,filter=this.filter(),hits=[];
  if(targetUp&&length(targetUp)>.9&&goal){
   // The observed/remembered target normal guides one foot probe. The caller
   // supplies the remembered goal; this helper never reads a hidden player pose.
   const n=norm(targetUp),face=physics.raycast(add(goal,mul(n,.75)),mul(n,-1),1.8,filter);
   if(this.faceValid(face)){
    const nearby=this.nearbyFace(this.referenceFrom(face));if(nearby)hits.push(nearby);
   }
  }
  const toward=goal?norm(sub(goal,center)):zero();
  if(length(toward)>.5){
   const hit=physics.raycast(center,toward,this.reach,filter);if(this.faceValid(hit))hits.push(hit);
   // Chest-level near-field search catches a wall before the creature reaches
   // its base. Sampling is staggered, not a per-creature world/body enumeration.
   const flat=norm(tangent(toward,u));
   if(length(flat)>.5){const hit=physics.raycast(add(center,mul(u,.45)),flat,this.reach,filter);
    if(this.faceValid(hit))hits.push(hit);}
  }
  const ordinary=this.ordinaryUp();
  // A roof can share the town's ordinary up. A floor below the creature is
  // not a route to a remembered target ABOVE it on that roof; returning there
  // every scan would alternate floor/wall forever instead of chasing upward.
  const returningDown=goal&&dot(sub(goal,this.entity.transform.position),ordinary)<.9;
  if(this.active&&returningDown&&(!targetUp||dot(targetUp,ordinary)>.8)){
   const floor=physics.raycast(center,mul(ordinary,-1),this.reach,filter);
   if(this.faceValid(floor))hits.push(floor);
  }
  // Stable tie ordering, with the remembered target frame preferred over a
  // coincidental obstacle. No assumption of humanoid/world Y is used here.
  const desired=targetUp&&length(targetUp)>.9?norm(targetUp):ordinary;
  hits.sort((a,b)=>dot(b.normal,desired)-dot(a.normal,desired)||a.distance-b.distance||String(a.entity.id).localeCompare(String(b.entity.id)));
  for(const hit of hits){
   if(dot(hit.normal,u)>.86)continue;
   // Already attached creatures change frame for the remembered destination,
   // not for an incidental ceiling/floor hit by a tangent chase probe. This
   // prevents repeated corner switching while hunting someone on the same wall.
   if(this.active&&dot(hit.normal,desired)<.65)continue;
   if(!this.active){
    // Ground creatures don't jump at ordinary furniture merely because it
    // blocks their nav route. There must be an elevated/inverted sensed goal.
    const elevated=goal&&dot(sub(goal,this.entity.transform.position),u)>1.15;
    const changed=targetUp&&dot(targetUp,u)<.72;
    if(!elevated&&!changed)continue;
    // A target already on a roof has ordinary up again; the vertical wall is
    // still a valid intermediate route. When its frame differs, prefer that
    // actual observed face rather than selecting an unrelated opposite wall.
    if(changed&&targetUp&&dot(hit.normal,norm(targetUp))<.65)continue;
   }
   if(this.suitable(hit))return hit;
  }
  return null;
 }

 select(hit){
  if(!this.suitable(hit))return false;
  if(!this.prior)this.prior=this.entity.gravity.state;
  this.reference=this.referenceFrom(hit);this.pending=null;
  this.from=this.entity.gravity.acceleration;const fromUp=length(this.from)>.01?norm(mul(this.from,-1)):this.entity.character.up;
  this.duration=Math.max(.15,Math.acos(clamp(dot(fromUp,this.reference.normal)))/(120*Math.PI/180));
  this.strength=Math.max(1,length(this.ordinaryAcceleration())||9.81);
  this.age=0;this.active=true;this.overridden=true;this.mode='turning';this.switches++;
  this.entity.gravity.setUniform(this.from);return true;
 }

 clear(reason='ordinary'){
  if(this.overridden&&this.entity.valid){const s=this.prior;
   if(s?.mode==='uniform')this.entity.gravity.setUniform(s.acceleration);
   else if(s?.mode==='field'&&s.source?.valid)this.entity.gravity.select(s.source);
   else this.entity.gravity.clear();
  }
  this.active=false;this.overridden=false;this.reference=null;this.pending=null;
  this.prior=null;this.mode=reason;this.age=0;this.lost=0;this.air=0;this.cooldown=1.2;
 }

 result(motion=null,launch=null){return {active:this.active,mode:this.mode,
  transitioning:this.mode==='jumping'||this.mode==='turning',reference:this.reference?.entity?.id||this.pending?.entity?.id||null,
  up:this.entity.valid?this.entity.character.up:null,motion,launch};}

 tick(dt,{goal=null,targetUp=null,target=null,pursue=false,speed=3}={}){
  if(!this.entity.valid||!this.entity.character)return this.result();
  this.targetEntity=target;const m=this.entity.character,u=m.up;
  this.cooldown=Math.max(0,this.cooldown-dt);this.scan-=dt;
  this.air=m.supported?0:this.air+dt;this.lost=pursue?0:this.lost+dt;

  if(this.pending){
   this.age+=dt;
   if(!this.refresh(this.pending)||this.age>1.05){this.clear('jump-blocked');return this.result();}
   // First leave support with an ordinary finite jump. Only then can the motor
   // rotate its capsule toward a clear wall, rather than intersecting a floor.
   this.pendingProbe-=dt;
   if(this.age>=.08&&!m.supported&&this.pendingProbe<=0){
    this.pendingProbe=.055;const hit=this.nearbyFace(this.pending);
    if(hit&&this.select(hit))return this.result(tangent(m.velocity,u));
   }
   const toward=tangent(mul(this.pending.normal,-1),u);
   return this.result(length(toward)>.05?mul(norm(toward),Math.min(4.3,speed+.6)):tangent(m.velocity,u));
  }

  if(this.overridden){
   if(!this.refresh(this.reference)){this.clear('reference-lost');return this.result();}
   this.age+=dt;
   const fraction=Math.min(1,this.age/this.duration),weight=fraction*fraction*(3-2*fraction);
   const targetAcceleration=mul(this.reference.normal,-this.strength);
   this.entity.gravity.setUniform(add(mul(this.from,1-weight),mul(targetAcceleration,weight)));
   const turning=fraction<1||dot(m.up,this.reference.normal)<.985;
   this.mode=turning?'turning':'surface';
   if(!turning&&dot(this.reference.normal,this.ordinaryUp())>.97&&m.supported){
    this.clear('ordinary');return this.result();
   }
   if(turning){
    // Retain jump/world motion while the up axis turns. The added approach is
    // bounded game intent, resolved by the same motor; no post-move correction.
    const approach=tangent(mul(this.reference.normal,-1),u);
    const carried=tangent(m.velocity,u);
    return this.result(length(approach)>.08&&length(carried)<speed?
     add(carried,mul(norm(approach),Math.min(.5,speed))):carried);
   }
   // A forgotten target doesn't grant eternal magnetic hovering. Only a real
   // nearby landing face may retain this reference after airborne separation.
   if(this.lost>2){this.clear('ordinary');return this.result();}
   if(this.air>1.25&&this.scan<=0){
    this.scan=.28+(this.seed%7)*.018;
    const landing=physics.raycast(this.center(),mul(this.reference.normal,-1),this.reach,this.filter());
    if(!this.faceValid(landing)){this.clear('ordinary');return this.result();}
   }
  }

  if(!pursue||this.cooldown>0||this.scan>0||!goal)return this.result();
  this.scan=.28+(this.seed%7)*.018;
  const hit=this.candidate(goal,targetUp);if(!hit)return this.result();
  if(this.active&&!m.supported){this.select(hit);return this.result(tangent(m.velocity,u));}
  if(m.supported){
   if(!this.prior)this.prior=this.entity.gravity.state;
   this.pending=this.referenceFrom(hit);this.active=true;this.mode='jumping';this.age=0;this.pendingProbe=0;this.jumps++;
   // Finite local-up takeoff, plus a separate bounded approach motion. This
   // does not silently erase existing velocity or provide repeated air jumps.
   const inward=tangent(mul(hit.normal,-1),u);
   return this.result(length(inward)>.05?mul(norm(inward),Math.min(4.3,speed+.6)):tangent(m.velocity,u),mul(u,5.4));
  }
  if(this.select(hit))return this.result(tangent(m.velocity,u));
  return this.result();
 }
 destroy(){this.clear('destroyed');}
}
