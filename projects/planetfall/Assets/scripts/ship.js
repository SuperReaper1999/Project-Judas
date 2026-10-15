// Enterable physical vessel. The motor owns walking; the rigid body owns flight.
// Seating is explicit control transfer. No player, planet or ship engine mode.
import {world,physics,input,scenes,session} from 'judas';
import {destinations} from './expedition.js';
import {add,sub,mul,dot,length,norm,qm,axis,rotate,tangent} from './math.js';
const Y={x:0,y:1,z:0},inverse=q=>({w:q.w,x:-q.x,y:-q.y,z:-q.z});
const clamp=(v,max)=>length(v)>max?mul(norm(v),max):v;
const cross=(a,b)=>({x:a.y*b.z-a.z*b.y,y:a.z*b.x-a.x*b.z,z:a.x*b.y-a.y*b.x});
function align(a,b){const d=Math.max(-1,Math.min(1,dot(a,b)));if(d>.99999)return {w:1,x:0,y:0,z:0};let c=cross(a,b);if(length(c)<.001)c=cross(a,{x:0,y:0,z:1});return axis(norm(c),Math.acos(d));}
const matrix=(m,v)=>add(add(mul(m.x,v.x),mul(m.y,v.y)),mul(m.z,v.z));
export default class Ship {
 constructor(player){this.p=player;this.hull=world.entity('3000');this.door=world.entity('3001');this.pilot=world.entity('3004');this.hinge=physics.joint(world.entity('3002'));this.piloting=false;this.open=false;this.inside=false;this.destination=1;this.cruise=false;this.prompt='';this.hold=this.hull.transform.position;this.attitude=this.hull.transform.rotation;}
 local(position){const t=this.hull.transform;return rotate(inverse(t.rotation),sub(position,t.position));}
 point(local,presented=false){const t=presented?this.hull.presentedTransform:this.hull.transform;return add(t.position,rotate(t.rotation,local));}
 restore(){if(!this.prior)return;const g=this.p.entity.gravity,s=this.prior;if(s.mode==='uniform')g.setUniform(s.acceleration);else if(s.mode==='field'&&s.source?.valid)g.select(s.source);else g.clear();this.prior=null;}
 seat(){this.pilot.renderVisible=true;this.p.grab.drop();this.p.boots.clear('seated');this.piloting=true;this.p.climb.active=false;this.p.weapon.aimBlend=0;this.p.entity.character.enabled=false;this.open=false;this.hinge.setSpring(0,450,90);this.p.state.yaw=0;this.p.state.pitch=0;this.hold=this.hull.transform.position;this.attitude=this.hull.transform.rotation;this.cruise=false;}
 stand(){if(length(this.hull.velocity)>2){this.exitWait=2;this.cruise=false;this.hold=add(this.hull.transform.position,mul(this.hull.velocity,1/3));return;}this.piloting=false;this.pilot.renderVisible=false;this.cruise=false;const t=this.hull.transform;this.p.entity.transform={position:this.point({x:1.15,y:.2,z:-.7}),rotation:t.rotation};this.p.entity.character.enabled=true;this.p.entity.character.velocity=this.hull.pointVelocity(this.p.entity.transform.position);this.p.state.yaw=0;this.p.state.pitch=0;}
 look(dt){if(!this.piloting)return false;const yaw=-(input.axis('look_x')*.0015+input.axis('look_stick_x')*1.3*dt),pitch=-(input.axis('look_y')*.0015+input.axis('look_stick_y')*1.3*dt);if(!this.cruise){this.attitude=qm(qm(this.attitude,axis(Y,yaw)),axis({x:1,y:0,z:0},pitch));const q=this.attitude,n=Math.hypot(q.w,q.x,q.y,q.z);this.attitude=mulQuaternion(q,1/n);}return true;}
 pose(presented=false){const t=presented?this.hull.presentedTransform:this.hull.transform;const local=this.p.camera.third?{x:0,y:5,z:11}:{x:0,y:1.45,z:-1.93};return {position:add(t.position,rotate(t.rotation,local)),rotation:t.rotation,direction:rotate(t.rotation,{x:0,y:0,z:-1})};}
 tick(dt){const hull=this.hull,t=hull.transform,position=this.p.entity.transform.position,local=this.local(position);
  const inside=Math.abs(local.x)<2.15&&local.y>-.2&&local.y<3.2&&Math.abs(local.z)<4.15;
  if(inside&&!this.inside){this.p.boots.clear('cabin');this.prior=this.p.entity.gravity.state;}
  if(inside||this.piloting)this.p.entity.gravity.setUniform(rotate(t.rotation,{x:0,y:-9.81,z:0}));else if(this.inside)this.restore();this.inside=inside;
  this.prompt='';this.exitWait=Math.max(0,(this.exitWait||0)-dt);let used=false;
  if(this.piloting){
   if(input.pressed('climb')){this.stand();used=true;}
   if(input.pressed('rotate')){this.destination=(this.destination+1)%destinations.length;this.cruise=false;}
   if(input.pressed('vendor')&&destinations[this.destination].scene!==scenes.current){const c=this.p.campaign;if(!(destinations[this.destination].scene.includes('earth')&&scenes.current.includes('earth'))&&!c.state.built.includes('ship'))c.dialogue='Repair the drive at Lantern first.';else if(c.state.resources.fuel<(destinations[this.destination].scene.includes('earth')&&scenes.current.includes('earth')?.2:c.state.choice==='reconnect'?.5:1))c.dialogue='No jump fuel. Complete mining contracts.';else this.jumpTime=8;}
   if(input.pressed('vendor')&&destinations[this.destination].scene===scenes.current){this.cruise=!this.cruise;this.stage='rise';this.rise=add(t.position,mul(rotate(t.rotation,Y),12));}
   if(input.pressed('reload')){const up=norm(mul(physics.gravity(t.position),-1));if(length(up)>.5)this.attitude=qm(align(rotate(t.rotation,Y),up),this.attitude);}
  }else{
   const nearDoor=length(sub(position,this.door.transform.position))<3.3;
   const nearSeat=inside&&length(sub(position,this.point({x:0,y:.2,z:-1.7})))<2;
   if(nearSeat)this.prompt='E  SIT AT COMMAND CONSOLE / PILOT';else if(nearDoor)this.prompt=`E  ${this.open?'CLOSE':'OPEN'} SHIP DOOR / WALK ABOARD`;
   if(input.pressed('climb')&&(nearSeat||nearDoor)){used=true;if(nearSeat)this.seat();else{this.open=!this.open;this.hinge.setSpring(this.open?1.5:0,450,90);}}
  }
  if(this.jumpTime){this.jumpTime=Math.max(0,this.jumpTime-dt);this.hold=add(t.position,rotate(t.rotation,{x:0,y:1,z:0}));this.p.campaign.dialogue=`Jump spool ${Math.ceil(this.jumpTime)}s / ${destinations[this.destination].name}`;if(this.jumpTime===0){this.p.campaign.state.resources.fuel-=destinations[this.destination].scene.includes('earth')&&scenes.current.includes('earth')?.2:this.p.campaign.state.choice==='reconnect'?.5:1;this.p.campaign.publish();session.set('planetfall-arrival',true);scenes.load(destinations[this.destination].scene);}}
  let desired={x:0,y:0,z:0};const moving=this.piloting&&(Math.abs(input.axis('move_x'))+Math.abs(input.axis('move_y'))>.08||input.held('jump')||input.held('aim'));
  if(moving){this.cruise=false;desired=rotate(t.rotation,clamp({x:input.axis('move_x'),y:(input.held('jump')?1:0)-(input.held('aim')?1:0),z:-input.axis('move_y')},1));desired=mul(desired,input.held('sprint')?50:20);this.hold=t.position;}
  else if(this.piloting&&this.cruise){
   const d=destinations[this.destination],approach=add(d.dock,rotate(d.frame||{w:1,x:0,y:0,z:0},{x:0,y:14,z:0}));let goal=this.stage==='rise'?this.rise:this.stage==='travel'?approach:d.dock;
   if(length(sub(goal,t.position))<(this.stage==='travel'?3:1)){if(this.stage==='rise')this.stage='travel';else if(this.stage==='travel')this.stage='land';else {this.cruise=false;this.hold=d.dock;}}
   desired=clamp(mul(sub(goal,t.position),.9),this.stage==='land'?5:42);
   // Dock orientation is ordinary project flight policy. It never changes gravity.
   this.attitude=d.frame||{w:1,x:0,y:0,z:0};this.hold=t.position;
  }else {if(this.wasMoving)this.hold=add(t.position,mul(hull.velocity,1/3));desired=clamp(mul(sub(this.hold,t.position),2),6);}this.wasMoving=moving;
  // Finite acceleration + ordinary forces, including sampled Judas gravity.
  const acceleration=clamp(mul(sub(desired,hull.velocity),3),24);
  hull.applyForce(mul(sub(acceleration,hull.gravity.acceleration),hull.mass));
  let error=qm(this.attitude,inverse(t.rotation));if(error.w<0)error=mulQuaternion(error,-1);
  const angular=clamp(sub(mul({x:error.x,y:error.y,z:error.z},18),mul(hull.angularVelocity,7)),10);
  hull.applyTorque(matrix(hull.inertiaWorld,angular));
  if(this.piloting){this.p.entity.transform={position:this.point({x:0,y:.2,z:-1.7}),rotation:t.rotation};const d=destinations[this.destination];this.prompt=this.exitWait>0?'RELEASE CONTROLS TO STOP, THEN E TO STAND':'E STAND | WASD | SPACE/CTRL LIFT | N CRUISE | T NEXT';}
  return this.piloting||used;
 }
 get status(){const d=destinations[this.destination];return `${d.name} / ${Math.round(length(sub(d.dock,this.hull.transform.position)))}m / ${Math.round(length(this.hull.velocity))}m/s${this.cruise?' / AUTO '+this.stage.toUpperCase():''}`;}
 get location(){return destinations.find(d=>d.scene===scenes.current)||null;}
 destroy(){this.restore();if(this.piloting&&this.p.entity.valid)this.p.entity.character.enabled=true;}
}
function mulQuaternion(q,s){return {w:q.w*s,x:q.x*s,y:q.y*s,z:q.z*s};}
