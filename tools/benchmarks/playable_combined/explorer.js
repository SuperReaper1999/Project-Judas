import {input,ui,world,scenes,saves,physics} from 'judas';
// Replaced from this project's authored motor geometry by prepare_playable_combined.py.
const BUILD_CAPSULES = [];
const TERRAIN_CELL = .5;
const add=(a,b)=>({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
const mul=(a,s)=>({x:a.x*s,y:a.y*s,z:a.z*s});
const size=a=>Math.hypot(a.x,a.y,a.z);
const norm=a=>size(a)>1e-6?mul(a,1/size(a)):{x:0,y:0,z:0};
const rotation=(yaw,pitch)=>{const cy=Math.cos(yaw/2),sy=Math.sin(yaw/2),cp=Math.cos(pitch/2),sp=Math.sin(pitch/2);return {w:cy*cp,x:cy*sp,y:sy*cp,z:-sy*sp}};
export default class {
 constructor({entity}){this.entity=entity;this.state={yaw:0,pitch:-.05,tool:'dig',layer:2,surfaceDabs:0,queue:[]};}
 start(){this.owner=world.entity('1');this.hud=ui.get('game_ui');
  input.pointerCapture=true;
  if(this.hud){this.hud.modal=false;for(const id of ['main','options','pause','image'])this.hud.get(id).visible=false;
   this.hud.get('crosshair').textAlignment='center';
   this.hud.get('hud').visible=true;this.hud.get('hud_title').text='COMBINED GAMEPLAY / INTERACTION WORKSHOP';
   this.hud.get('hud_help').text='WASD + mouse: explore | Space: jump | T: tunnel\n1: dig | 2: raise | 3: lower | 4: paint | 5: next layer\nLeft click: selected tool | Right click: add material\nF6/F7: save/load | R: reset | Esc: pause';}
  this.state.tool??='dig';this.state.layer??=2;this.state.surfaceDabs??=0;
  this.state.queue??=[];
  if(!this.owner.terrain.state?.ready)throw new Error('Native terrain was not initialized; check terrain.asset on the landscape.');
 }
 restore(){this.start();console.log('TERRAIN_V2_RESTORED '+JSON.stringify({position:this.entity.transform.position,terrain:this.owner.terrain.state,surfaceDabs:this.state.surfaceDabs,remainingBrushes:this.state.queue.length}));}
 view(){const p=this.entity.presentedTransform.position;world.setView({position:add(p,{x:0,y:.65,z:0}),rotation:rotation(this.state.yaw,this.state.pitch)},70);}
 canAdd(point,radius){
  // New solid must not enclose a motor. Include a full cell diagonal for
  // the sampled terrain boundary; do not move/reset the character to hide it.
  for(const authored of BUILD_CAPSULES){const e=world.entity(authored.id),motor=e?.character;if(!motor)continue;
   const p=e.transform.position,q=e.transform.rotation,v=authored.offset;
   const uv={x:q.y*v.z-q.z*v.y,y:q.z*v.x-q.x*v.z,z:q.x*v.y-q.y*v.x};
   const uuv={x:q.y*uv.z-q.z*uv.y,y:q.z*uv.x-q.x*uv.z,z:q.x*uv.y-q.y*uv.x};
   const center=add(p,add(v,add(mul(uv,2*q.w),mul(uuv,2)))),up=motor.up,d=add(point,mul(center,-1));
   const along=Math.max(-authored.halfHeight,Math.min(authored.halfHeight,d.x*up.x+d.y*up.y+d.z*up.z));
   if(size(add(d,mul(up,-along)))<=radius+authored.radius+authored.skin+Math.sqrt(3)*TERRAIN_CELL)return false;
  }return true;
 }
 presentationUpdate(){this.view();}
 update(dt){if(this.hud?.modal)return;
  this.state.yaw-=input.axis('look_x')*.0021;this.state.pitch=Math.max(-1.45,Math.min(1.45,this.state.pitch-input.axis('look_y')*.0021));
  if(input.pressed('reset'))scenes.reload();
  for(const tool of ['dig','raise','lower','paint'])if(input.pressed('mode_'+tool))this.state.tool=tool;
  if(input.pressed('paint_layer'))this.state.layer=(this.state.layer+1)%4;
  if(input.pressed('save')){console.log('TERRAIN_V2_SAVE_REQUEST '+JSON.stringify({remainingBrushes:this.state.queue.length,busy:this.owner.terrain.state.busy}));this.request=saves.save('highlands');}if(input.pressed('load'))this.request=saves.load('highlands');
  if(input.pressed('tunnel')&&!this.state.queue.length)for(let z=10;z>=-10;z-=1.1)this.state.queue.push({point:{x:0,y:2.2,z},kind:'remove',radius:1.65});
  if(input.pressed('dig')||input.pressed('build')){this.view();const {origin,direction}=world.viewRay;
   const hit=physics.raycast(origin,direction,12,{ignored:[this.entity]});
   if(hit?.entity?.id===this.owner.id){const surface=!input.pressed('build')&&this.state.tool!=='dig';
    this.state.queue.push({point:hit.point,surface,kind:surface?this.state.tool:(input.pressed('build')?'add':'remove'),radius:surface?2:1.35,layer:this.state.layer});}
  }
  const s=this.owner.terrain.state;if(s?.ready&&!s.busy&&this.state.queue.length){const b=this.state.queue[0];
   if(b.surface){if(this.owner.terrain.surfaceBrush(b.point,{kind:b.kind,radius:b.radius,strength:b.kind==='paint'?1:.25,layer:b.layer})){this.state.queue.shift();this.state.surfaceDabs++;console.log('TERRAIN_V2_SURFACE_DAB '+b.kind);}}
   else if(b.kind==='add'&&!this.canAdd(b.point,b.radius)){this.state.queue.shift();this.state.notice='Cannot add terrain through a character. Aim farther away.';console.log('TERRAIN_BUILD_BLOCKED '+JSON.stringify(b.point));}
   else if(this.owner.terrain.brush(b.point,{kind:b.kind,radius:b.radius})){this.state.queue.shift();this.state.notice='';console.log('TERRAIN_BRUSH_APPLIED '+JSON.stringify({kind:b.kind,point:b.point}));}}
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
  let text=s?`${s.ready?'Ready':'Preparing terrain'} | revision ${s.revision} | ${s.chunks} chunks / ${s.volumeChunks} excavated\n${s.navigation?'Live navigation ready':'Navigation disabled'} | ${s.triangles} triangles | ${(s.bytes/1048576).toFixed(1)} MiB geometry\nPrepare ${s.prepareMs.toFixed(2)} ms (worker) | publish ${s.publishMs.toFixed(2)} ms\n${s.error|| (this.state.queue.length?'Excavating tunnel: '+this.state.queue.length+' cuts remaining':'Press T, then walk through the hillside. Geometry and collision change together.')}`:'Preparing source';
  if(this.state.notice)text+='\n'+this.state.notice;
  text+='\nTool: '+this.state.tool+' | layer '+(this.state.layer+1)+' | saved surface dabs '+this.state.surfaceDabs;
  if(this.request){const r=saves.status(this.request);text+='\nSave/load: '+r?.state+' '+(r?.error||'');}
  this.hud.get('counter').text=text;
 }
 onUI(e){if(e.type==='back'||(e.type==='click'&&e.element==='resume')){this.hud.modal=false;this.hud.get('pause').visible=false;}
  if(e.type==='click'&&e.element==='pause_options')scenes.reload();if(e.type==='click'&&e.element==='pause_quit')ui.quit();}
 destroy(){input.pointerCapture=false;world.clearView();}
}
