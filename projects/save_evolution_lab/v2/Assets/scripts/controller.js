import {world, input, ui, scenes, saves, profiler, physics} from 'judas';

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
 constructor({entity}){this.entity=entity;this.state={yaw:0,pitch:0,tool:0,quarry:0,inventory:{ore:0}};this.message='SAVE EVOLUTION — V2. Collect salvage (C), dig/build/travel, then save progress.';}
 start(){this.bind();} 
 restore(){this.bind();}
 bind(){this.view=null;this.hud=ui.get('terrain_lab');this.hud.modal=false;this.hud.get('pause').visible=false;ui.debugOverlayVisible=false;input.pointerCapture=true;}
 uiUpdate(){if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}}
 onUI(e){if(e.type==='click'&&e.element==='resume'){this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;}if(e.type==='click'&&e.element==='reload')scenes.reload();}
 update(){if(this.hud.modal)return;this.state.yaw-=input.axis('look_x')*.0021;this.state.pitch=Math.max(-1.4,Math.min(1.4,this.state.pitch-input.axis('look_y')*.0021));
  if(input.pressed('collect'))this.state.inventory.ore+=1;
  if(input.pressed('reset'))scenes.reload();
  if(input.pressed('tool'))this.state.tool=(this.state.tool+1)%3;
  if(input.pressed('travel')){this.state.quarry=(this.state.quarry+1)%3;this.travelPending=true;}
  if(input.pressed('flat')){this.state.quarry=0;this.travelPending=true;}
  if(input.pressed('rotated')){this.state.quarry=2;this.travelPending=true;}
  if(input.pressed('origin')){try{const o=world.origin;world.requestOrigin({x:o.x+512,y:o.y,z:o.z});this.message='Shifted chart; absolute terrain and suspended state stay anchored.';}catch(e){this.message=String(e);}}
  const region='quarry'+this.state.quarry;
  if(!this.token||this.tokenRegion!==region){if(this.token)scenes.releaseRegion(this.token);this.token=scenes.requestRegion(region);this.tokenRegion=region;}
  // Position interests support ordinary walking away and back, independent of F3 travel.
  scenes.setInterest('traveller',this.entity.transform.position,{load:8,retain:15});
  if(input.pressed('dig')&&this.view){const hit=physics.raycast(this.view.position,this.view.direction,10,{ignored:[this.entity]});
   if(hit?.entity?.terrain.state?.ready&&!hit.entity.terrain.state.busy){try{const kind=['remove','add','smooth'][this.state.tool],point=add(hit.point,mul(hit.normal,kind==='add'?.5:-.7));hit.entity.terrain.brush(point,{kind,radius:1.8,strength:kind==='smooth'?.5:1});this.message=kind+' queued; last complete geometry remains until publication.';}catch(e){this.message=String(e);}}
   else this.message='Aim at a ready quarry surface within 10 metres.';
  }
  if(input.pressed('build')){try{const t=this.entity.transform,heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));t.position=add(t.position,rotate(heading,{x:0,y:1,z:-3}));const block=world.spawnPrefab('447760bfcc12bf1c58786519546688af',t);scenes.adopt(block,region);this.message='Physical block built and adopted by '+region;}catch(e){this.message=String(e);}}
  if(input.pressed('destroy')&&this.view){const hit=physics.raycast(this.view.position,this.view.direction,10,{ignored:[this.entity]});if(hit?.entity&&hit.entity.mass>0){const joint=physics.joint(hit.entity);if(joint){joint.destroy();this.message='Released actual load-bearing joint; beam falls physically.';}else{hit.entity.destroy();this.message='Removed physical object; tombstone retained.';}}}
  if(input.pressed('save'))this.request=saves.save('v2-progress');if(input.pressed('exact'))this.request=saves.load('old-progress');
  if(input.pressed('nextload'))this.request=saves.load('v2-progress',{migrate:true});
  if(input.pressed('refresh'))this.request=saves.refresh();
  if(input.pressed('load'))this.request=saves.load('old-progress', {migrate:true});
  if(input.pressed('drop')){const t=this.entity.transform,heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));t.position=add(t.position,rotate(heading,{x:0,y:.8,z:-1.5}));const e=world.spawnPrefab(drop,t);if(e)e.applyImpulse(rotate(heading,{x:0,y:2,z:-4}));}
 }
 fixedUpdate(dt){if(this.hud.modal)return;const region='quarry'+this.state.quarry;
  if(this.travelPending&&scenes.regionStatus(this.token)?.state==='active'){
   const rock=scenes.resolveRegionEntity(region,1),r=rock.transform.rotation;
   this.entity.transform={position:add(rock.transform.position,rotate(r,{x:0,y:-3.6,z:11})),rotation:r};this.entity.character.velocity={x:0,y:0,z:0};this.state.yaw=0;this.state.pitch=0;this.travelPending=false;
  }
const motor=this.entity.character,state=motor.state,up=motor.up,t=this.entity.transform;const heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));
  const x=Number(input.held('move_right'))-Number(input.held('move_left')),z=Number(input.held('move_forward'))-Number(input.held('move_back'));
  const desired=mul(norm(add(mul(tangent(rotate(heading,{x:1,y:0,z:0}),up),x),mul(tangent(rotate(heading,{x:0,y:0,z:-1}),up),z))),5);
  let velocity=motor.velocity;
  if(state.supported){velocity=add(desired,add(state.supportVelocity,mul(up,dot(velocity,up))));if(input.pressed('jump'))velocity=add(velocity,mul(up,5));}
  else {const change=add(desired,mul(tangent(velocity,up),-1));velocity=add(velocity,mul(change,Math.min(1,3*dt)));}
  motor.velocity=velocity;profiler.counter('M98 evolution traveller',1,'latest');
 }
 presentationUpdate(){const t=this.entity.presentedTransform,heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));const position=add(t.position,rotate(t.rotation,{x:0,y:.65,z:0})),rotation=qm(heading,axis({x:1,y:0,z:0},this.state.pitch));this.view={position,direction:rotate(rotation,{x:0,y:0,z:-1})};world.setView({position,rotation},70,{near:.05,far:250});
  const rock=scenes.resolveRegionEntity('quarry'+this.state.quarry,1),terrain=rock?.terrain.state,stats=scenes.streamingStats;
  let text=`SAVE EVOLUTION V2 | SALVAGE: ${this.state.inventory?.ore} | CHAPTER: ${this.state.chapter||"legacy"}\nTOOL: ${['REMOVE','ADD','SMOOTH'][this.state.tool]} | ${this.message}\n${terrain?.busy?'PREPARING':terrain?.ready?'READY':'LOADING'} | revision ${terrain?.revision||0} | ${terrain?.triangles||0} triangles | terrain ${terrain?.prepareMs.toFixed(1)||0} ms prep / ${terrain?.publishMs.toFixed(2)||0} ms publish\n${scenes.regions.map(r=>r.id+': '+r.state+(r.pins.length?' ['+r.pins.join(', ')+']':'')).join(' | ')}\n${stats?.active||0} active regions | ${((stats?.retainedBytes||0)/1024).toFixed(0)} KiB retained | absolute origin ${world.origin.x.toFixed(0)}\n${terrain?.error||''}`;
  if(this.request){const r=saves.status(this.request);text+=`\nSave/load ${r?.state} | migrated ${r?.migrated} | ${r?.migrationMs?.toFixed(2)} ms | ${r?.error||''}`;}this.hud.get('status').text=text;

 }
 destroy(){if(this.token)scenes.releaseRegion(this.token);scenes.removeInterest('traveller');input.pointerCapture=false;world.clearView();}
}
