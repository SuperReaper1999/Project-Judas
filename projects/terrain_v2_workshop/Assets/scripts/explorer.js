import {input,ui,world,scenes,saves,physics} from 'judas';
import landscape from '../terrain/terrain-source.js';
const add=(a,b)=>({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
const mul=(a,s)=>({x:a.x*s,y:a.y*s,z:a.z*s});
const size=a=>Math.hypot(a.x,a.y,a.z);
const norm=a=>size(a)>1e-6?mul(a,1/size(a)):{x:0,y:0,z:0};
const rotation=(yaw,pitch)=>{const cy=Math.cos(yaw/2),sy=Math.sin(yaw/2),cp=Math.cos(pitch/2),sp=Math.sin(pitch/2);return {w:cy*cp,x:cy*sp,y:sy*cp,z:-sy*sp}};
export default class {
 constructor({entity}){this.entity=entity;this.state={yaw:0,pitch:-.05};this.queue=[];}
 start(){this.owner=world.entity('1');this.hud=ui.get('game_ui');
  ui.debugOverlayVisible=false;input.pointerCapture=true;
  if(this.hud){this.hud.modal=false;for(const id of ['main','options','pause','image'])this.hud.get(id).visible=false;
   this.hud.get('hud').visible=true;this.hud.get('hud_title').text='TERRAIN V2 / HIGHLANDS WORKSHOP';
   this.hud.get('hud_help').text='WASD + mouse: explore | Space: jump\nT: excavate a real tunnel through the hill\nLeft click: dig | Right click: add material\nF6/F7: save/load | R: reset | Esc: pause';}
  if(!this.owner.terrain.state)this.owner.terrain.createLandscape(JSON.stringify(landscape));
 }
 restore(){this.start();}
 view(){const p=this.entity.presentedTransform.position;world.setView({position:add(p,{x:0,y:.65,z:0}),rotation:rotation(this.state.yaw,this.state.pitch)},70);}
 presentationUpdate(){this.view();}
 update(dt){if(this.hud?.modal)return;
  this.state.yaw-=input.axis('look_x')*.0021;this.state.pitch=Math.max(-1.45,Math.min(1.45,this.state.pitch-input.axis('look_y')*.0021));
  if(input.pressed('reset'))scenes.reload();
  if(input.pressed('save'))this.request=saves.save('highlands');if(input.pressed('load'))this.request=saves.load('highlands');
  if(input.pressed('tunnel')&&!this.queue.length)for(let z=10;z>=-10;z-=1.1)this.queue.push({point:{x:0,y:2.2,z},kind:'remove',radius:1.65});
  if(input.pressed('dig')||input.pressed('build')){const origin=add(this.entity.transform.position,{x:0,y:.65,z:0});
   const cp=Math.cos(this.state.pitch),direction={x:-Math.sin(this.state.yaw)*cp,y:Math.sin(this.state.pitch),z:-Math.cos(this.state.yaw)*cp};
   const hit=physics.raycast(origin,direction,7,{ignored:[this.entity]});
   if(hit?.entity?.id===this.owner.id)this.queue.push({point:hit.point,kind:input.pressed('build')?'add':'remove',radius:1.35});
  }
  const s=this.owner.terrain.state;if(s?.ready&&!s.busy&&this.queue.length){const b=this.queue.shift();this.owner.terrain.brush(b.point,{kind:b.kind,radius:b.radius});}
 }
 fixedUpdate(dt){const motor=this.entity.character;if(!motor)return;
  if(!this.owner.terrain.state?.ready){motor.velocity={x:0,y:0,z:0};return;}
  const sy=Math.sin(this.state.yaw),cy=Math.cos(this.state.yaw);
  let direction={x:cy*input.axis('move_x')-sy*input.axis('move_y'),y:0,z:-sy*input.axis('move_x')-cy*input.axis('move_y')};
  if(size(direction)>1)direction=norm(direction);let v=motor.velocity;
  const change={x:direction.x*4-v.x,y:0,z:direction.z*4-v.z};
  v=add(v,mul(norm(change),Math.min(size(change),dt*(motor.state.supported?28:10))));
  if(input.pressed('jump')&&motor.state.supported)v.y=5;motor.velocity=v;
 }
 uiUpdate(){if(!this.hud)return;if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;}
  input.pointerCapture=!this.hud.modal;this.hud.get('crosshair').visible=!this.hud.modal;
  const s=this.owner.terrain.state;
  let text=s?`${s.ready?'Ready':'Preparing terrain'} | revision ${s.revision} | ${s.chunks} chunks / ${s.volumeChunks} excavated\n${s.triangles} triangles | ${(s.bytes/1048576).toFixed(1)} MiB geometry\nPrepare ${s.prepareMs.toFixed(2)} ms (worker) | publish ${s.publishMs.toFixed(2)} ms\n${s.error|| (this.queue.length?'Excavating tunnel: '+this.queue.length+' cuts remaining':'Press T, then walk through the hillside. Geometry and collision change together.')}`:'Preparing source';
  if(this.request){const r=saves.status(this.request);text+='\nSave/load: '+r?.state+' '+(r?.error||'');}
  this.hud.get('counter').text=text;
 }
 onUI(e){if(e.type==='back'||(e.type==='click'&&e.element==='resume')){this.hud.modal=false;this.hud.get('pause').visible=false;}
  if(e.type==='click'&&e.element==='pause_options')scenes.reload();if(e.type==='click'&&e.element==='pause_quit')ui.quit();}
 destroy(){input.pointerCapture=false;world.clearView();}
}
