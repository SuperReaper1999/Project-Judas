import {input,ui,world,scenes,physics} from 'judas';
export const properties={speed:{type:'number',default:4},launchSpeed:{type:'number',default:5}};
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
 constructor({entity,properties}){this.entity=entity;this.props=properties;this.state={yaw:0,pitch:0};}
 start(){this.hud=ui.get('game_ui');ui.debugOverlayVisible=false;this.hud.modal=false;
  for(const id of ['main','options','pause','image'])this.hud.get(id).visible=false;
  this.hud.get('hud').visible=true;this.hud.get('hud_title').text='YOUR GRAVITY / SAME WORLD';
  this.hud.get('hud_help').text='WASD + mouse: move/look | Space: jump | G: aim + choose floor\nF: spatial gravity | C: uniform field | T: RadicalGravity\nR: reload | Escape: pause';
  this.hud.get('pause_options').text='Reload gravity room';input.pointerCapture=true;
 }
 update(dt){if(this.hud.modal)return;
  this.state.yaw-=input.axis('look_x')*.0021+input.axis('look_stick_x')*2.2*dt;
  this.state.pitch=Math.max(-1.5,Math.min(1.5,this.state.pitch-input.axis('look_y')*.0021-input.axis('look_stick_y')*2.2*dt));
  if(input.pressed('reset'))scenes.reload();
 }
 presentationUpdate(){if(this.hud.modal)return;
  const t=this.entity.presentedTransform;
  const rotation=qm(qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));
  world.setView({position:add(t.position,rotate(t.rotation,{x:0,y:.65,z:0})),rotation},70);
 }
 fixedUpdate(dt){
  const motor=this.entity.character;if(this.hud.modal)return;
  // Policy is project JS: point at a real surface and choose its inward gravity.
  if(input.pressed('interact')){const ray=world.viewRay;
   const hit=ray&&physics.raycast(ray.origin,ray.direction,30,{ignored:[this.entity],includeSensors:false});
   if(hit)this.entity.gravity.setUniform(mul(hit.normal,-9.81));
  }
  if(input.pressed('control_toggle'))this.entity.gravity.clear();
  if(input.pressed('igniter'))this.entity.gravity.select(world.entity('6'));
  if(input.pressed('torch_toggle'))this.entity.gravity.select(world.entity('3'));
  const s=motor.state,up=motor.up,t=this.entity.transform;
  const heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));
  const forward=norm(tangent(rotate(heading,{x:0,y:0,z:-1}),up));
  const right=norm(tangent(rotate(heading,{x:1,y:0,z:0}),up));
  let direction=add(mul(forward,input.axis('move_y')),mul(right,input.axis('move_x')));if(length(direction)>1)direction=norm(direction);
  const desired=mul(direction,this.props.speed);let velocity=motor.velocity;
  if(s.supported){const own=tangent(add(velocity,mul(s.supportVelocity,-1)),up),change=add(desired,mul(own,-1));
   velocity=add(s.supportVelocity,add(own,mul(norm(change),Math.min(length(change),24*dt))));
   if(input.pressed('jump'))velocity=add(velocity,mul(up,this.props.launchSpeed));
  }else if(length(direction)>0){const speed=dot(tangent(velocity,up),norm(direction));if(speed<this.props.speed)velocity=add(velocity,mul(norm(direction),Math.min(8*dt,this.props.speed-speed)));}
  motor.velocity=velocity;
 }
 menu(open){this.hud.modal=open;this.hud.get('pause').visible=open;input.pointerCapture=!open;}
 uiUpdate(){if(input.pressed('pause'))this.menu(!this.hud.modal);
  const g=this.entity.gravity.state,s=this.entity.character.state;
  this.hud.get('counter').text=`${g.mode} | g=(${g.acceleration.x.toFixed(1)}, ${g.acceleration.y.toFixed(1)}, ${g.acceleration.z.toFixed(1)}) m/s²\nSupported: ${s.supported} | blue prop: spatial | gold prop: ceiling`;
 }
 onUI(e){if(e.type==='back'||(e.type==='click'&&e.element==='resume'))this.menu(false);
  if(e.type==='click'&&e.element==='pause_options')scenes.reload();if(e.type==='click'&&e.element==='pause_quit')ui.quit();}
 destroy(){input.pointerCapture=false;world.clearView();}
}
