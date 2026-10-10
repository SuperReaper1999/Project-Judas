import {world, input, ui, scenes, saves, profiler} from 'judas';

const add=(a,b)=>({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
const mul=(v,s)=>({x:v.x*s,y:v.y*s,z:v.z*s});
const dot=(a,b)=>a.x*b.x+a.y*b.y+a.z*b.z;
const norm=v=>{const n=Math.sqrt(dot(v,v));return n>1e-6?mul(v,1/n):{x:0,y:0,z:0};};
const tangent=(v,up)=>add(v,mul(up,-dot(v,up)));
const qm=(a,b)=>({w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w});
const axis=(v,a)=>({w:Math.cos(a/2),...mul(v,Math.sin(a/2))});
const rotate=(q,v)=>{const r=qm(qm(q,{w:0,...v}),{w:q.w,x:-q.x,y:-q.y,z:-q.z});return {x:r.x,y:r.y,z:r.z};};
const drop='ca7848d55e08f6bb5a042f38dbc8a311';

// Game intent and camera policy live here. The motor samples ordinary gravity;
// neither this script nor the terrain knows a universal world-up direction.
export default class {
 constructor({entity}){this.entity=entity;this.state={yaw:0,pitch:0};}
 start(){this.bind();}
 restore(){this.bind();}
 bind(){this.hud=ui.get('terrain_lab');this.hud.modal=false;this.hud.get('pause').visible=false;ui.debugOverlayVisible=false;input.pointerCapture=true;}
 uiUpdate(){if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}}
 onUI(e){if(e.type==='click'&&e.element==='resume'){this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;}if(e.type==='click'&&e.element==='reload')scenes.reload();}
 update(){if(this.hud.modal)return;this.state.yaw-=input.axis('look_x')*.0021;this.state.pitch=Math.max(-1.4,Math.min(1.4,this.state.pitch-input.axis('look_y')*.0021));
  if(input.pressed('flat'))scenes.load('Scenes/landscape.judas');if(input.pressed('rotated'))scenes.load('Scenes/rotated.judas');if(input.pressed('reset'))scenes.reload();
  if(input.pressed('save'))this.request=saves.save('terrain-round');if(input.pressed('load'))this.request=saves.load('terrain-round');
  if(input.pressed('drop')){const t=this.entity.transform,heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));t.position=add(t.position,rotate(heading,{x:0,y:.8,z:-1.5}));const e=world.spawnPrefab(drop,t);if(e)e.applyImpulse(rotate(heading,{x:0,y:2,z:-4}));}
 }
 fixedUpdate(dt){if(this.hud.modal)return;const motor=this.entity.character,state=motor.state,up=motor.up,t=this.entity.transform;const heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));
  const x=Number(input.held('move_right'))-Number(input.held('move_left')),z=Number(input.held('move_forward'))-Number(input.held('move_back'));
  const desired=mul(norm(add(mul(tangent(rotate(heading,{x:1,y:0,z:0}),up),x),mul(tangent(rotate(heading,{x:0,y:0,z:-1}),up),z))),5);
  let velocity=motor.velocity;
  if(state.supported){velocity=add(desired,add(state.supportVelocity,mul(up,dot(velocity,up))));if(input.pressed('jump'))velocity=add(velocity,mul(up,5));}
  else {const change=add(desired,mul(tangent(velocity,up),-1));velocity=add(velocity,mul(change,Math.min(1,3*dt)));}
  motor.velocity=velocity;profiler.counter('M75 terrain traveller',1,'latest');
 }
 presentationUpdate(){const t=this.entity.presentedTransform,heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));world.setView({position:add(t.position,rotate(t.rotation,{x:0,y:.65,z:0})),rotation:qm(heading,axis({x:1,y:0,z:0},this.state.pitch))},70,{near:.05,far:200});
  const m=this.entity.character,s=m.state;let text=`${scenes.current}\nSupport ${s.supported} | speed ${Math.sqrt(dot(s.velocity,s.velocity)).toFixed(2)} m/s\nGravity ${m.gravity.x.toFixed(2)}, ${m.gravity.y.toFixed(2)}, ${m.gravity.z.toFixed(2)} | finite edited mesh`;if(this.request){const r=saves.status(this.request);text+=`\nSave/load ${r?.state} ${r?.error||''}`;}this.hud.get('status').text=text;
 }
 destroy(){input.pointerCapture=false;world.clearView();}
}
