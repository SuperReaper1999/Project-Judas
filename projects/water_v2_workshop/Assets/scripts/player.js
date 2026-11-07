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
 start(){this.held=null;this.entity.character?.ignore([]);this.hud=ui.get('game_ui');ui.debugOverlayVisible=false;input.pointerCapture=true;
  if(this.hud){this.hud.modal=false;for(const id of ['main','options','pause','image'])this.hud.get(id).visible=false;this.hud.get('hud').visible=true;
   this.hud.get('hud_title').text='WATER V2 / SOURCE PREVIEW';
   this.hud.get('hud_help').text='WASD + mouse | Space: jump / swim | Q: dive\nG: grab / release | H: throw | F: pump | B: splash\n1: pool | 2: rotated | 3: radial | 4: original planet | F6/F7: save/load | Esc: pause';
   this.hud.get('pause_options').text='Restart walk';}
  this.camera(this.entity.transform);
 }
 restore(){this.start();console.log('WATER_WORKSHOP_RESTORED '+JSON.stringify({pump:this.state.pump,water:world.gridWaterRegions[0]?.gridWater?.state}));} // reacquire UI; saved yaw/pitch are already restored
 camera(pose){const rotation=qm(qm(pose.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));
  world.setView({position:add(pose.position,rotate(pose.rotation,{x:0,y:.65,z:0})),rotation},70);}
 update(dt){if(this.hud?.modal)return;
  this.state.yaw-=input.axis('look_x')*.0021+input.axis('look_stick_x')*2.2*dt;
  this.state.pitch=Math.max(-1.45,Math.min(1.45,this.state.pitch-input.axis('look_y')*.0021-input.axis('look_stick_y')*2.2*dt));
  if(input.pressed('reset'))scenes.reload();
  for(const [key,path] of [['pool','pool'],['rotated','rotated'],['radial','radial'],['original','planet-basin']])
   if(input.pressed(key))scenes.load('Scenes/'+path+'.judas');
  if(input.pressed('save'))this.request=saves.save('water-workshop');
  if(input.pressed('load'))this.request=saves.load('water-workshop');
  if(input.pressed('control_toggle'))this.state.pump=!this.state.pump;
  if(input.pressed('interact')){
   if(this.held){this.held=null;this.entity.character.ignore([]);}
   else{const p=this.entity.transform,rotation=qm(qm(p.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));
    const eye=add(p.position,rotate(p.rotation,{x:0,y:.65,z:0}));
    const hit=physics.raycast(eye,rotate(rotation,{x:0,y:0,z:-1}),5);
    if(hit?.entity?.motionType==='dynamic'){this.held=hit.entity;this.entity.character.ignore([this.held]);}}
  }
  if(input.pressed('throw')&&this.held?.valid){
   const p=this.entity.transform,r=qm(qm(p.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));
   this.held.applyImpulseAtPoint(mul(rotate(r,{x:0,y:0,z:-1}),this.held.mass*6),this.held.transform.position);
   this.held=null;this.entity.character.ignore([]);
  }}
 presentationUpdate(){this.camera(this.entity.presentedTransform);}
 fixedUpdate(dt){const motor=this.entity.character;if(!motor)return;
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
   if(input.pressed('add_water'))waterHandle.impulse(point({x:0,y:1.9,z:2}),rotate(frame.rotation,{x:0,y:65,z:0}),1.2);
  }
  if(this.held?.valid){
   const rotation=qm(heading,axis({x:1,y:0,z:0},this.state.pitch));
   const target=add(add(pose.position,mul(up,.45)),rotate(rotation,{x:0,y:0,z:-2.2}));
   const change=add(mul(add(target,mul(this.held.transform.position,-1)),45),mul(this.held.velocity,-11));
   this.held.applyForce(mul(add(change,mul(this.held.gravity.acceleration,-1)),this.held.mass));
  }
 }
 uiUpdate(){if(!this.hud)return;if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;}
  input.pointerCapture=!this.hud.modal;
  const s=world.gridWaterRegions[0]?.gridWater?.state;
  let text=s?`Water ${s.quantity.toFixed(3)} m³ | stored ${s.storedDisplacement.toFixed(3)} m³ | swim ${Math.round(this.immersion*100)}%\n${s.simulationMs.toFixed(2)} ms incl. geometry/body loads | pump ${this.state.pump?'ON':'OFF'}\n${s.error||'Grid owns quantity; mesh is presentation only.'}`:'Advanced Fluids locked or disabled; use an Experimental source build.';
  if(this.request){const r=saves.status(this.request);text+='\nSave/load: '+r?.state+' '+(r?.error||'');}
  this.hud.get('counter').text=text;}
 onUI(e){if(e.type==='back'||(e.type==='click'&&e.element==='resume')){this.hud.modal=false;this.hud.get('pause').visible=false;}
  if(e.type==='click'&&e.element==='pause_options')scenes.reload();if(e.type==='click'&&e.element==='pause_quit')ui.quit();}
 destroy(){if(this.entity.valid)this.entity.character.ignore([]);input.pointerCapture=false;world.clearView();}
}
