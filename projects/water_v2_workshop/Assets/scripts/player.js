import {input,ui,world,scenes,saves,physics} from 'judas';
const add=(a,b)=>({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
const mul=(a,s)=>({x:a.x*s,y:a.y*s,z:a.z*s});
const dot=(a,b)=>a.x*b.x+a.y*b.y+a.z*b.z;
const length=a=>Math.sqrt(dot(a,a));
const norm=a=>length(a)>1e-6?mul(a,1/length(a)):{x:0,y:0,z:0};
const tangent=(a,u)=>add(a,mul(u,-dot(a,u)));
const qm=(a,b)=>({w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w});
const axis=(a,t)=>({w:Math.cos(t/2),...mul(a,Math.sin(t/2))});
const rotate=(q,v)=>{const r=qm(qm(q,{w:0,...v}),{w:q.w,x:-q.x,y:-q.y,z:-q.z});return {x:r.x,y:r.y,z:r.z}};
export default class {
 constructor({entity}){this.entity=entity;this.state={yaw:0,pitch:-.24,pump:false};this.immersion=0;}
 start(){this.held=null;this.feedback='Aim the crosshair at a loose prop, then press G.';this.entity.character?.ignore([]);this.hud=ui.get('game_ui');input.pointerCapture=true;
  if(this.hud){this.hud.modal=false;for(const id of ['main','options','pause','image'])this.hud.get(id).visible=false;this.hud.get('hud').visible=true;
   this.hud.get('hud_title').text='WATER V2 / SOURCE PREVIEW';
   this.hud.get('hud_help').text='WASD + mouse | Space: jump / swim | Q: dive (in water)\nG: grab / release aimed prop | H: throw held prop\nF: water circulation | B: splash where you aim\n1/2/3/4: scenes | F6/F7: save/load | Esc: pause | R: restart';
   this.hud.get('pause_options').text='Restart walk';}
  this.camera(this.entity.transform);
 }
 restore(){this.start();console.log('WATER_WORKSHOP_RESTORED '+JSON.stringify({pump:this.state.pump,water:world.gridWaterRegions[0]?.gridWater?.state}));} // reacquire UI; saved yaw/pitch are already restored
 camera(pose){const rotation=qm(qm(pose.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));
  world.setView({position:add(pose.position,rotate(pose.rotation,{x:0,y:.65,z:0})),rotation},70);}
 ray(){const pose=this.entity.transform;
  const rotation=qm(qm(pose.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));
  return {origin:add(pose.position,rotate(pose.rotation,{x:0,y:.65,z:0})),direction:rotate(rotation,{x:0,y:0,z:-1})};}
 target(){const ray=this.ray();
  // The camera is inside our motor capsule. Never let it intercept its own ray.
  return physics.raycast(ray.origin,ray.direction,5,{ignored:[this.entity]});}
 release(){this.held=null;this.entity.character?.ignore([]);}
 grab(){if(this.held?.valid){this.release();this.feedback='Released the prop.';return;}
  const hit=this.target();
  if(hit?.entity?.motionType==='dynamic'){this.held=hit.entity;this.entity.character.ignore([this.held]);this.feedback='Picked up a prop. G: release / H: throw.';}
  else this.feedback=hit?.entity?'That is fixed scenery; aim at a loose prop.':'No prop in reach. Aim at one within 5 metres.';}
 throw(){if(!this.held?.valid){this.release();this.feedback='Pick up a prop with G before throwing.';return;}
  const ray=this.ray();
  this.held.applyImpulseAtPoint(mul(ray.direction,this.held.mass*6),this.held.transform.position);
  this.release();this.feedback='Threw the prop.';}
 splash(water){const ray=this.ray(),up=this.entity.character.up;
  const right=rotate(this.entity.transform.rotation,{x:1,y:0,z:0});
  const obstacle=physics.raycast(ray.origin,ray.direction,12,{ignored:[this.entity]});
  const reach=obstacle?obstacle.distance:12;
  // B is a deliberate local disturbance, not a hidden fixed point in the pool.
  // Stop at real solid geometry; do not splash water through a wall or prop.
  for(let distance=.2;distance<reach;distance+=.2){
   const point=add(ray.origin,mul(ray.direction,distance));
   if(world.fluidSample(point,up,0,.08,right).immersion>.05){
    const accepted=water.impulse(point,mul(up,350),.6);
    this.feedback=accepted?'Splashed the water at the crosshair.':'No physical water at that point.';return;
   }
  }
  this.feedback='Aim down at nearby water to splash (up to 12 metres).';}
 update(dt){if(this.hud?.modal)return;
  this.state.yaw-=input.axis('look_x')*.0021+input.axis('look_stick_x')*2.2*dt;
  this.state.pitch=Math.max(-1.45,Math.min(1.45,this.state.pitch-input.axis('look_y')*.0021-input.axis('look_stick_y')*2.2*dt));
  if(input.pressed('reset'))scenes.reload();
  for(const [key,path] of [['pool','pool'],['rotated','rotated'],['radial','radial'],['original','planet-basin']])
   if(input.pressed(key))scenes.load('Scenes/'+path+'.judas');
  if(input.pressed('save'))this.request=saves.save('water-workshop');
  if(input.pressed('load'))this.request=saves.load('water-workshop');
 }
 presentationUpdate(){this.camera(this.entity.presentedTransform);}
 fixedUpdate(dt){const motor=this.entity.character;if(!motor)return;
  if(input.pressed('interact'))this.grab();
  if(input.pressed('throw'))this.throw();
  if(input.pressed('control_toggle')){this.state.pump=!this.state.pump;this.feedback='Water circulation '+(this.state.pump?'ON: adding and draining equal quantities.':'OFF.');}
  const state=motor.state,up=motor.up,pose=this.entity.transform;
  const heading=qm(pose.rotation,axis({x:0,y:1,z:0},this.state.yaw));
  const forward=norm(tangent(rotate(heading,{x:0,y:0,z:-1}),up)),right=norm(tangent(rotate(heading,{x:1,y:0,z:0}),up));
  let direction=add(mul(forward,input.axis('move_y')),mul(right,input.axis('move_x')));
  if(length(direction)>1)direction=norm(direction);
  let velocity=motor.velocity;
  if(state.supported){const support=state.supportVelocity,own=tangent(add(velocity,mul(support,-1)),up);
   const change=add(mul(direction,4),mul(own,-1));velocity=add(support,add(own,mul(norm(change),Math.min(length(change),24*dt))));
   if(input.pressed('jump'))velocity=add(velocity,mul(up,5));
  }else if(length(direction)>0){const speed=dot(tangent(velocity,up),norm(direction));if(speed<4)velocity=add(velocity,mul(norm(direction),Math.min(8*dt,4-speed)));}
  // The same public query samples prescribed basic water with no fluid solver.
  const water=world.fluidSample(pose.position,up,.6,.3,right);this.immersion=water.immersion;
  if(water.immersion>0){
   const buoyancy=mul(add(motor.gravity,mul(water.acceleration,-1)),-water.density/950*water.immersion);
   const look=rotate(qm(heading,axis({x:1,y:0,z:0},this.state.pitch)),{x:0,y:0,z:-1});
   let swimming=add(mul(look,input.axis('move_y')),mul(right,input.axis('move_x')));
   swimming=add(swimming,mul(up,(input.held('jump')?1:0)+Math.min(0,input.axis('move_z'))));
   const relaxation=Math.exp(-2*water.immersion*dt);velocity=add(water.velocity,mul(add(velocity,mul(water.velocity,-1)),relaxation));
   motor.accelerate(add(buoyancy,mul(norm(swimming),6*water.immersion)));
  }
  motor.velocity=velocity;
  const owner=world.gridWaterRegions[0],waterHandle=owner?.gridWater;
  if(waterHandle){const frame=owner.transform;
   const point=v=>add(frame.position,rotate(frame.rotation,v));
   if(this.state.pump){waterHandle.changeVolume(point({x:-3,y:2.6,z:-1}),.15*dt,.8);
    waterHandle.changeVolume(point({x:3,y:.8,z:0}),-.15*dt,.8);}
   if(input.pressed('add_water'))this.splash(waterHandle);
  }
  if(this.held?.valid){
   const rotation=qm(heading,axis({x:1,y:0,z:0},this.state.pitch));
   const target=add(add(pose.position,mul(up,.45)),rotate(rotation,{x:0,y:0,z:-2.2}));
   const change=add(mul(add(target,mul(this.held.transform.position,-1)),45),mul(this.held.velocity,-11));
   this.held.applyForce(mul(add(change,mul(this.held.gravity.acceleration,-1)),this.held.mass));
  }
 }
 uiUpdate(){if(!this.hud)return;if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;}
  this.hud.get('crosshair').visible=!this.hud.modal;
  input.pointerCapture=!this.hud.modal;
  const s=world.gridWaterRegions[0]?.gridWater?.state;
  let text=s?`Water ${s.quantity.toFixed(3)} m³ | stored ${s.storedDisplacement.toFixed(3)} m³ | swim ${Math.round(this.immersion*100)}%\n${s.simulationMs.toFixed(2)} ms incl. geometry/body loads | pump ${this.state.pump?'ON':'OFF'}\n${s.error||'Grid owns quantity; mesh is presentation only.'}`:'Advanced Fluids locked or disabled; use an Experimental source build.';
  const target=!this.held?.valid?this.target():null;
  text+='\n'+(this.held?.valid?'Holding a prop | G: release / H: throw':target?.entity?.motionType==='dynamic'?'G: pick up this prop':'Aim at a loose prop within 5 metres.');
  text+='\n'+this.feedback;
  if(this.request){const r=saves.status(this.request);text+='\nSave/load: '+r?.state+' '+(r?.error||'');}
  this.hud.get('counter').text=text;}
 onUI(e){if(e.type==='back'||(e.type==='click'&&e.element==='resume')){this.hud.modal=false;this.hud.get('pause').visible=false;}
  if(e.type==='click'&&e.element==='pause_options')scenes.reload();if(e.type==='click'&&e.element==='pause_quit')ui.quit();}
 destroy(){if(this.entity.valid)this.entity.character.ignore([]);input.pointerCapture=false;world.clearView();}
}
