// Optional equipment policy. Judas owns gravity, capsule motion and collision;
// this project selects references from actual geometry and never teleports or
// assigns velocity while changing frames. No universal up is used here.
import {physics,input,signals} from 'judas';
import {game} from './game.js';
import {add,sub,mul,dot,length,norm,rotate,axis,qm} from './math.js';
const cross=(a,b)=>({x:a.y*b.z-a.z*b.y,y:a.z*b.x-a.x*b.z,z:a.x*b.y-a.y*b.x});
const inverse=q=>({w:q.w,x:-q.x,y:-q.y,z:-q.z});
const clamp=v=>Math.max(-1,Math.min(1,v));
function align(a,b){const cosine=clamp(dot(a,b));if(cosine>.99999)return {w:1,x:0,y:0,z:0};
 let ax=cross(a,b);if(length(ax)<1e-5)ax=cross(a,Math.abs(a.x)<.8?{x:1,y:0,z:0}:{x:0,y:0,z:1});
 return axis(norm(ax),Math.acos(cosine));}
export default class Framewalk {
 constructor(player,options={}){this.player=player;this.options={strength:9.81,reach:7,recoveryRange:24,turnSpeed:150,radius:.3,halfHeight:.6,offset:.9,...options};
  this.active=false;this.armed=false;this.age=0;this.airTime=0;this.scanTime=0;this.up=null;this.target=null;this.wasSupported=false;this.previousVelocity=null;
  this.state=player.state.boots={equipped:false,active:false,transitioning:false,mode:'ordinary',reference:null,switches:0,recoveries:0,failsafes:0,lastMomentumDelta:0};}
 get equipped(){return this.player.inventory?.equipped('feet')==='framewalk_boots';}
 filter(){const held=this.player.grab?.held;return {ignored:[this.player.entity,...(held?.valid?[held]:[])],excludedTags:['enemy'],includeSensors:false};}
 center(){const t=this.player.entity.transform;return add(t.position,rotate(t.rotation,{x:0,y:this.options.offset,z:0}));}
 suitable(hit){if(!hit||!hit.normal||!hit.entity?.valid||!['static','kinematic'].includes(hit.entity.motionType))return false;
  const n=norm(hit.normal),center=this.center();if(length(n)<.9||dot(n,sub(center,hit.point))<.02)return false;
  const t=this.player.entity.transform,rotation=qm(align(rotate(t.rotation,{x:0,y:1,z:0}),n),t.rotation);
  const landingCenter=add(hit.point,mul(n,this.options.halfHeight+this.options.radius+.065));
  // A reference must have space for our actual configured capsule, not merely a
  // render face. Clearance and approach use the same production query geometry.
  if(physics.capsuleCast({position:landingCenter,rotation},this.options.radius,this.options.halfHeight,n,0,this.filter()))return false;
  const delta=sub(hit.point,center),distance=length(delta);
  if(distance>.02){const visible=physics.raycast(center,norm(delta),distance+.04,this.filter());if(visible&&visible.distance<distance-.075)return false;}
  const approach=sub(landingCenter,center),travel=length(approach);
  if(travel>.02){const obstruction=physics.capsuleCast({position:center,rotation:t.rotation},this.options.radius,this.options.halfHeight,norm(approach),travel,this.filter());
   if(obstruction&&obstruction.distance<travel-.035){
    // Existing motor recovery may leave a skin-scale initial observation while
    // turning near a corner. A clear destination moving OUT of that contact is
    // valid; inward/tangential blocked approaches remain rejected.
    if(!obstruction.initialOverlap||dot(norm(approach),obstruction.normal)<.05)return false;}}
  return true;
 }
 select(hit,recovery=false){if(!this.suitable(hit))return false;const e=this.player.entity,m=e.character;
  const before=m.velocity;if(!this.active)this.priorGravity=e.gravity.state;
  this.fromAcceleration=e.gravity.acceleration;this.up=norm(mul(this.fromAcceleration,-1));if(length(this.up)<.5)this.up=m.up;
  this.duration=Math.max(.1,Math.acos(clamp(dot(this.up,norm(hit.normal))))/(this.options.turnSpeed*Math.PI/180));
  const reference=hit.entity.transform;this.target={normal:norm(hit.normal),point:hit.point,entity:hit.entity,
   localNormal:rotate(inverse(reference.rotation),norm(hit.normal)),localPoint:rotate(inverse(reference.rotation),sub(hit.point,reference.position))};this.active=true;this.age=0;this.scanTime=.35;
  // Assign intent only. The existing motor reorients its capsule at its authored
  // rate and the camera consumes interpolated presentation transforms.
  e.gravity.setUniform(this.fromAcceleration);const after=m.velocity;
  this.state.lastMomentumDelta=length(sub(after,before));this.state.active=true;this.state.transitioning=true;this.state.mode=recovery?'recovery':'switch';
  this.state.reference=hit.entity.id;this.state.point=hit.point;this.state.normal=hit.normal;
  if(recovery)this.state.recoveries++;else this.state.switches++;
  game.message=recovery?'FRAMEWALK: nearest clear surface recovered':'FRAMEWALK: changing floor - world momentum retained';return true;
 }
 clear(reason='ordinary',failsafe=false){if(this.active&&this.player.entity.valid){const e=this.player.entity,before=e.character.velocity,s=this.priorGravity;
   // The optional equipment temporarily owns selection, not authored gravity.
   // Restore the prior owner intent; ordinary spatial selection is the default
   // and also the fallback when a previously selected source has disappeared.
   if(s?.mode==='uniform')e.gravity.setUniform(s.acceleration);else if(s?.mode==='field'&&s.source?.valid)e.gravity.select(s.source);else e.gravity.clear();
   this.state.lastMomentumDelta=length(sub(e.character.velocity,before));}this.active=false;this.target=null;this.priorGravity=null;this.armed=false;
  this.state.active=false;this.state.transitioning=false;this.state.reference=null;this.state.mode=reason;if(failsafe)this.state.failsafes++;
  if(reason==='no-reference')game.message='FRAMEWALK: no clear recovery surface - normal authored gravity resumed';}
 launch(){this.armed=this.equipped;this.airTime=0;}
 airborneJump(){if(!this.equipped||!this.armed||this.airTime<.06||this.player.climb.active)return false;
  const view=this.player.camera.pose(),origin=add(this.player.entity.transform.position,mul(this.player.entity.character.up,1.15));
  // The second Space press is a geometric selection, not another upward impulse.
  const hit=physics.raycast(origin,view.direction,this.options.reach,this.filter());
  if(!this.select(hit)){game.message='FRAMEWALK: aim toward a clear wall, ceiling or floor within 7 m';return false;}
  this.armed=false;return true;
 }
 surface(hit){if(!hit?.entity?.valid)return null;const center=this.center();
  // Nearest-point edge normals are separation directions, not necessarily a
  // flat face's normal. Aim slightly into the queried geometry, then use the
  // authoritative ray's actual face normal for a gravity reference.
  const inset=mul(norm(sub(hit.entity.transform.position,hit.point)),.08),delta=sub(add(hit.point,inset),center),distance=length(delta);
  if(distance<.01)return null;const face=physics.raycast(center,norm(delta),distance+.12,this.filter());
  return face?.entityId===hit.entityId?face:null;
 }
 recovery(){const center=this.center(),range=this.options.recoveryRange,filter=this.filter();
  // Ask for nearest actual surfaces, excluding rejected body candidates. This
  // avoids a tiny loose prop hiding an otherwise nearer suitable wall/floor.
  const rejected=[];for(let i=0;i<8;i++){const nearest=physics.closestPoint(center,range,{...filter,ignored:[...filter.ignored,...rejected]});
   if(!nearest)break;const face=this.surface(nearest);if(this.suitable(face))return this.select(face,true);
   if(!nearest.entity?.valid)break;rejected.push(nearest.entity);}
  // Bounded fallback samples local directions. No distant entity enumeration or
  // jumping across occluded/dry spaces; every result is a visible physical face.
  const local=[{x:0,y:-1,z:0},{x:0,y:1,z:0},{x:1,y:0,z:0},{x:-1,y:0,z:0},{x:0,y:0,z:1},{x:0,y:0,z:-1}];
  for(const x of [-1,1])for(const y of [-1,1])for(const z of [-1,1])local.push(norm({x,y,z}));
  const rotation=this.player.entity.transform.rotation,rays=local.map(d=>({origin:center,direction:rotate(rotation,d),maximum:range}));
  const hits=physics.raycastMany(rays,filter).filter(h=>this.suitable(h)).sort((a,b)=>a.distance-b.distance||String(a.entityId).localeCompare(String(b.entityId)));
  if(hits.length)return this.select(hits[0],true);this.clear('no-reference',true);return false;
 }
 tick(dt){const e=this.player.entity,m=e.character;this.state.equipped=this.equipped;
  if(m.supported){if(!this.wasSupported&&this.previousVelocity){const impact=Math.max(0,-dot(sub(this.previousVelocity,m.supportVelocity),m.supportNormal));
    if(impact>9)signals.send(e,'player.hurt',{damage:Math.min(100,Math.ceil((impact-9)*8)),kind:'fall'});}
   this.airTime=0;this.armed=false;}else this.airTime+=dt;
  this.wasSupported=m.supported;this.previousVelocity=m.velocity;
  if(!this.equipped){if(this.active)this.clear('unequipped');return;}
  if(input.pressed('gravity_reset')){this.clear('released');game.message='FRAMEWALK released - normal authored gravity';return;}
  if(!this.active)return;this.age+=dt;this.scanTime-=dt;
  if(!this.target?.entity.valid||!this.target.entity.collider?.enabled){this.recovery();return;}
  const reference=this.target.entity.transform;this.target.normal=norm(rotate(reference.rotation,this.target.localNormal));
  this.target.point=add(reference.position,rotate(reference.rotation,this.target.localPoint));
  // Blend acceleration, rather than rotating a full-strength force sideways.
  // Opposite references pass briefly through zero gravity; the motor retains
  // its up near zero and turns at its authored rate once the new direction wins.
  // This gives a smooth field transition without an arbitrary lateral impulse,
  // a velocity assignment or a transform correction in project code.
  const progress=Math.min(1,this.age/this.duration),weight=progress*progress*(3-2*progress);
  const targetAcceleration=mul(this.target.normal,-this.options.strength);
  e.gravity.setUniform(add(mul(this.fromAcceleration,1-weight),mul(targetAcceleration,weight)));
  this.up=norm(mul(e.gravity.acceleration,-1));if(length(this.up)<.5)this.up=m.up;
  this.state.transitioning=progress<1||dot(m.up,this.target.normal)<.997;
  if(!this.state.transitioning)this.state.mode='surface';
  if(m.supported||this.state.transitioning||this.age<.85||this.airTime<.3||this.scanTime>0||this.player.climb.active)return;
  this.scanTime=.3;const down=mul(this.up,-1),landing=physics.raycast(this.center(),down,this.options.recoveryRange,this.filter());
  if(!this.suitable(landing))this.recovery();
 }
 destroy(){this.clear('destroyed');}
}
