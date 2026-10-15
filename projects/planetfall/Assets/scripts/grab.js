import {world,physics,input} from 'judas';
import {game} from './game.js';
import {add,sub,mul,dot,length,norm,qm,axis,rotate} from './math.js';
const bounded=(v,max)=>length(v)>max?mul(v,max/length(v)):v;
const cross=(a,b)=>({x:a.y*b.z-a.z*b.y,y:a.z*b.x-a.x*b.z,z:a.x*b.y-a.y*b.x});
const inverse=q=>({w:q.w,x:-q.x,y:-q.y,z:-q.z});
function attitude(target,current){let q=qm(target,inverse(current));if(q.w<0)q={w:-q.w,x:-q.x,y:-q.y,z:-q.z};
 const v={x:q.x,y:q.y,z:q.z},s=length(v);return s>.00001?mul(v,2*Math.atan2(s,Math.max(0,q.w))/s):{x:0,y:0,z:0};}
const matrix=(m,v)=>add(add(mul(m.x,v.x),mul(m.y,v.y)),mul(m.z,v.z));
// The carried object remains an ordinary finite-mass body. This bounded force
// grip can collide, lag behind and be broken; no transform parenting/teleports.
export default class Grab {
 constructor(player){this.player=player;this.held=null;this.turn=0;this.target=null;}
 restoreGravity(){const e=this.held,s=this.savedGravity;if(!e?.valid||!this.gravityInherited||!s)return;
  if(s.mode==='uniform')e.gravity.setUniform(s.acceleration);else if(s.mode==='field'&&s.source?.valid)e.gravity.select(s.source);else e.gravity.clear();this.gravityInherited=false;}
 drop(){this.restoreGravity();if(this.held)game.message='PROP RELEASED - its ordinary physics continues';if(this.held&&this.player.entity.valid)this.player.entity.character.ignore([]);this.held=null;this.player.state.holding=null;}
 acquire(){const view=this.player.camera.pose(),filter={ignored:[this.player.entity]};
  const sight=physics.raycast(view.position,view.direction,4.5,filter),target=sight?sight.point:add(view.eye,mul(view.direction,3.2));
  const direction=norm(sub(target,view.eye)),hit=physics.raycast(view.eye,direction,3.2,filter),e=hit?.entity;
  if(!e?.valid||!e.hasTag('physical')||e.motionType!=='dynamic'||e.mass>35||e.collider.sensor){game.message='B: aim at a nearby crate or barrel to carry it';return false;}
  this.held=e;this.player.entity.character.ignore([e]);this.startYaw=this.player.state.yaw;this.startRotation=e.transform.rotation;this.startHeading=qm(this.player.entity.transform.rotation,axis({x:0,y:1,z:0},this.startYaw));this.savedGravity=e.gravity.state;this.gravityInherited=false;this.direction=view.direction;this.turn=0;this.player.state.holding=e.id;
  game.message='CARRYING - look down to drag / B drop / T quarter turn';return true;
 }
 tick(dt){if(input.pressed('interact')){if(this.held)this.drop();else if(!this.player.melee.busy&&!this.player.climb.active)this.acquire();}
  const e=this.held;if(!e)return;
  if(!e.valid||e.motionType!=='dynamic'||!e.collider.enabled){this.drop();return;}
  const view=this.player.camera.pose(),u=this.player.entity.character.up;
  if(this.player.boots?.active){e.gravity.setUniform(this.player.entity.gravity.acceleration);this.gravityInherited=true;}else this.restoreGravity();
  // Turning the grip follows an arc, instead of commanding a crate straight
  // through the holder on a rapid 180-degree look. Forces remain bounded.
  const cosine=Math.max(-1,Math.min(1,dot(this.direction,view.direction))),angle=Math.acos(cosine);
  let turnAxis=norm(cross(this.direction,view.direction));if(length(turnAxis)<.001)turnAxis=u;
  if(angle>.001)this.direction=norm(rotate(axis(turnAxis,Math.min(angle,3.6*dt)),this.direction));
  this.target=add(add(view.eye,mul(this.direction,2.05)),mul(u,-.35));
  if(length(sub(e.transform.position,this.target))>4.5){this.drop();return;}
  if(input.pressed('rotate'))this.turn+=Math.PI/2;
  const error=sub(this.target,e.transform.position),relative=sub(this.player.entity.character.velocity,e.velocity);
  const acceleration=bounded(sub(add(mul(error,70),mul(relative,17)),e.gravity.acceleration),45);
  e.applyForce(mul(acceleration,e.mass));
  const heading=qm(this.player.entity.transform.rotation,axis({x:0,y:1,z:0},this.player.state.yaw));
  const targetRotation=qm(axis(u,this.turn),qm(qm(heading,inverse(this.startHeading)),this.startRotation));
  const alpha=bounded(sub(mul(attitude(targetRotation,e.transform.rotation),40),mul(e.angularVelocity,12)),60);
  e.applyTorque(matrix(e.inertiaWorld,alpha));
 }
 destroy(){this.drop();}
}
