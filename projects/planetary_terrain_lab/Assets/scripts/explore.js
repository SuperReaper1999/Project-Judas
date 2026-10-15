import {world,input,ui,scenes,saves} from 'judas';
import {add,sub,mul,dot,length,norm,cross,tangent,look} from './math.js';
const PLANET='1';
// Binary64 absolute positions are project-owned navigation state. The motor,
// physics casts and render poses use world.toLocal at the last boundary only.
export default class {
 constructor(){this.state={mode:'landing',direction:{x:0,y:1,z:0},altitude:1.7,yaw:0,pitch:0,actor:null,balls:[],tour:false,tourTime:0};}
 start(){this.bind();}
 restore(){this.bind();}
 bind(){this.planet=world.entity(PLANET);this.hud=ui.get('terrain_lab');this.hud.modal=false;this.hud.get('pause').visible=false;ui.debugOverlayVisible=false;input.pointerCapture=true;this.problem='';}
 surface(direction){const s=this.planet.planetState;return this.planet.planetSample(add(s.center,mul(direction,s.radius)));}
 get eye(){if(this.state.actor){const a=world.entity(this.state.actor);if(a?.valid)return add(world.toAbsolute(a.presentedTransform.position),mul(this.state.direction,.7));}return add(this.surface(this.state.direction).position,mul(this.state.direction,this.state.altitude));}
 leaveGround(){this.state.altitude=Math.max(.1,this.planet.planetSample(this.eye).distance);if(this.state.actor){const a=world.entity(this.state.actor);if(a?.valid){this.state.direction=norm(sub(world.toAbsolute(a.transform.position),this.planet.planetState.center));a.destroy();}this.state.actor=null;}for(const id of this.state.balls){const b=world.entity(id);if(b?.valid)b.destroy();}this.state.balls=[];scenes.removeInterest('planet-traveller');this.planet.clearPlanetCollision();this.state.mode='flight';}
 uiUpdate(){if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}}
 onUI(e){if(e.type==='click'&&e.element==='resume'){this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;}if(e.type==='click'&&e.element==='reload')scenes.reload();}
 update(dt){if(this.hud.modal)return;this.state.yaw+=input.axis('look_x')*.002;this.state.pitch=Math.max(-1.4,Math.min(1.4,this.state.pitch-input.axis('look_y')*.002));
  if(input.pressed('orbit')){this.leaveGround();this.state.mode='ascending';this.state.tour=false;}
  if(input.pressed('land')){this.leaveGround();this.state.mode='descending';this.state.tour=false;}
  if(input.pressed('edge')){this.leaveGround();this.state.direction=norm({x:1,y:1,z:0});this.state.altitude=1200;this.state.mode='descending';this.state.tour=false;}
  if(input.pressed('tour')){this.leaveGround();this.state.altitude=4000000;this.state.mode='descending';this.state.tour=true;this.state.tourTime=0;this.state.yaw=0;this.state.pitch=0;}
  if(input.pressed('reset'))scenes.reload();if(input.pressed('save'))this.request=saves.save('planet');if(input.pressed('load'))this.request=saves.load('planet');
  if(input.pressed('drop')&&this.state.actor){const pose=world.entity(this.state.actor).transform,up=this.state.direction,basis=this.basis();const ball=world.spawnPrefab('23533a0ff5561c9f634ceacfe1353ee6',{position:add(pose.position,add(mul(up,1),mul(basis.forward,2))),rotation:pose.rotation});if(ball){ball.applyImpulse(add(mul(basis.forward,8),mul(up,4)));this.state.balls.push(ball.id);}}
  if(this.state.mode==='ascending')this.state.altitude=Math.min(4000000,this.state.altitude+Math.max(30,this.state.altitude*1.4)*dt);
  if(this.state.mode==='descending'){this.state.altitude=Math.max(1.7,this.state.altitude*Math.exp(-.4*dt));if(this.state.altitude<=1.701)this.state.mode='landing';}
  if(this.state.mode==='flight'||this.state.mode==='ascending'){const side=this.basis().right;const speed=dt*.08*(Number(input.held('move_right'))-Number(input.held('move_left')));this.state.direction=norm(add(this.state.direction,mul(side,speed)));}
 }
 basis(){const up=this.state.direction;const base=norm(tangent(Math.abs(up.z)<.9?{x:0,y:0,z:-1}:{x:1,y:0,z:0},up));const right=cross(base,up);return {forward:add(mul(base,Math.cos(this.state.yaw)),mul(right,Math.sin(this.state.yaw))),right:add(mul(right,Math.cos(this.state.yaw)),mul(base,-Math.sin(this.state.yaw)))};}
 fixedUpdate(dt){if(this.hud.modal)return;
  if(this.state.mode==='landing'){
   const sample=this.surface(this.state.direction),absolute=add(sample.position,mul(this.state.direction,1.0));
   try{if(length(sub(absolute,world.origin))>1000){if(!world.originState.pending)world.requestOrigin(absolute);return;}
    if(!this.planet.preparePlanetCollision(absolute)){this.problem="Preparing local collision tiles…";return;}const actor=world.spawnPrefab('dcdf293d630f1245eadc3f44c7010014',{position:world.toLocal(absolute),rotation:look(this.basis().forward,this.state.direction)});if(!actor)throw Error('traveller prefab unavailable');this.state.actor=actor.id;this.state.mode='ground';this.problem='';scenes.setInterest('planet-traveller',actor.transform.position,{load:32,retain:64,priority:5});
   }catch(error){this.problem=String(error);}return;
  }
  if(this.state.mode!=='ground'){try{if(length(world.toLocal(this.eye))>1500&&!world.originState.pending)world.requestOrigin(this.eye);this.problem="";}catch(error){this.problem=String(error);}return;}const actor=world.entity(this.state.actor);if(!actor?.valid)return;scenes.setInterest('planet-traveller',actor.transform.position,{load:32,retain:64,priority:5});const motor=actor.character,absolute=world.toAbsolute(actor.transform.position);this.state.direction=norm(sub(absolute,this.planet.planetState.center));
  try{if(!this.planet.preparePlanetCollision(absolute)){motor.velocity={x:0,y:0,z:0};return;}if(length(actor.transform.position)>1500&&!world.originState.pending)world.requestOrigin(absolute);this.problem='';}catch(error){this.problem=String(error);motor.velocity={x:0,y:0,z:0};return;}
  const b=this.basis(),up=this.state.direction,desired=mul(norm(add(mul(b.right,Number(input.held('move_right'))-Number(input.held('move_left'))),mul(b.forward,Number(input.held('move_forward'))-Number(input.held('move_back'))))),5);let velocity=motor.velocity;
  if(motor.supported){velocity=add(desired,mul(up,dot(velocity,up)));if(input.pressed('jump'))velocity=add(velocity,mul(up,5));}else velocity=add(velocity,mul(sub(desired,tangent(velocity,up)),Math.min(1,dt*3)));motor.velocity=velocity;
  if(this.state.tour){this.state.tourTime+=dt;motor.velocity=add(mul(b.right,3),mul(up,dot(motor.velocity,up)));if(this.state.tourTime>8)this.state.tour=false;}
 }
 presentationUpdate(){const eye=this.eye,b=this.basis(),up=this.state.direction;let forward=add(mul(b.forward,Math.cos(this.state.pitch)),mul(up,Math.sin(this.state.pitch)));
  if(this.state.mode!=='ground'){const blend=Math.max(0,Math.min(1,(Math.log(Math.max(1,this.state.altitude))-Math.log(10))/(Math.log(500)-Math.log(10))));forward=norm(add(mul(forward,1-blend),mul(norm(add(mul(up,-1),mul(b.forward,.12))),blend)));}
  const near=this.state.mode==='ground'?.05:Math.max(.05,this.state.altitude*.002),far=Math.max(200000,this.state.altitude+15000000);
  world.setView({position:world.toLocal(eye),rotation:look(forward,Math.abs(dot(forward,up))>.99?b.forward:up)},70,{near,far});
  const s=this.planet.planetState,altitude=this.planet.planetSample(eye).distance;let text=`Earth radius ${(s.radius/1000).toFixed(0)} km | ${this.state.mode} | altitude ${altitude.toFixed(1)} m\nTiles ${s.leaves} / resident ${s.resident} | triangles ${s.triangles} | collider tiles ${s.colliders}\nLOD target error ${s.maximumPixelError.toFixed(2)} px${s.budgetLimited?' (budget/depth limited)':''} | preparation ${s.prepareMs.toFixed(2)} ms\nAbsolute ${eye.x.toFixed(3)}, ${eye.y.toFixed(3)}, ${eye.z.toFixed(3)} | origin revision ${world.originState.revision}\nRadicalGravity / real mesh contact / 2.5 cm spectral detail + authored 12 cm patch`;
  if(this.state.actor)text+=`\nSupported ${world.entity(this.state.actor).character.supported} | local chart ${length(world.entity(this.state.actor).transform.position).toFixed(1)} m`;
  if(this.request){const r=saves.status(this.request);text+=`\nSave ${r?.state} ${r?.error||''}`;}if(this.problem)text+='\n'+this.problem;this.hud.get('status').text=text;
 }
 destroy(){if(this.planet?.valid)this.planet.clearPlanetCollision();input.pointerCapture=false;world.clearView();}
}
