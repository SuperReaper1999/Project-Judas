import {world,input,ui,scenes,saves,localization} from 'judas';
export const properties={speed:{type:'number',default:7},prefab:{type:'string',default:''},rig:{type:'entity',default:null}};
const add=(a,b)=>({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
const mul=(a,s)=>({x:a.x*s,y:a.y*s,z:a.z*s});
const qm=(a,b)=>({w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w});
const axis=(a,t)=>({w:Math.cos(t/2),...mul(a,Math.sin(t/2))});
const rotate=(q,v)=>{let r=qm(qm(q,{w:0,...v}),{w:q.w,x:-q.x,y:-q.y,z:-q.z});return {x:r.x,y:r.y,z:r.z}};
export default class {
 constructor({entity,properties}){this.entity=entity;this.props=properties;this.state={yaw:0,pitch:0,third:false,traveller:'',spawns:0};this.elapsed=0;}
 start(){this.bind();}
 restore(){this.bind();}
 bind(){this.hud=ui.get('workshop');ui.debugOverlayVisible=false;this.hud.modal=false;input.pointerCapture=true;this.hud.get('pause').visible=false;this.projectCamera();}
 uiUpdate(){
  this.openedThisUIFrame=false;
  if(input.pressed('pause')){this.openedThisUIFrame=!this.hud.modal;this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}
  if(input.pressed('save'))this.request=saves.save('workshop',{name:'M67 workshop'});
  if(input.pressed('load'))this.request=saves.load('workshop');
  if(input.pressed('locale'))localization.setLocale(localization.locale==='ar'?'en':'ar');
  if(this.request){const result=saves.status(this.request);this.hud.get('status').text=`Save/load: ${result?.state}${result?.error?' — '+result.error:''}`;}
 }
 update(dt){if(this.hud.modal)return;
  this.state.yaw-=input.axis('look_x')*.0021+input.axis('look_stick_x')*dt*2;
  this.state.pitch=Math.max(-1.45,Math.min(1.45,this.state.pitch-input.axis('look_y')*.0021-input.axis('look_stick_y')*dt*2));
  if(input.pressed('view_toggle'))this.state.third=!this.state.third;
  if(input.pressed('reset'))scenes.reload();
  if(input.pressed('spawn')){
   const t=this.entity.transform;const forward=rotate(axis({x:0,y:1,z:0},this.state.yaw),{x:0,y:0,z:-1});
   const entity=world.spawnPrefab(this.props.prefab,{position:add(t.position,add(mul(forward,2),{x:0,y:1,z:0}))},{velocity:mul(forward,6),angularVelocity:{x:0,y:1,z:0},scripts:[{slot:1,properties:{label:`Travelling instance ${++this.state.spawns}`,owner:{entity:this.entity.id}},state:{serial:this.state.spawns}}]});
   this.state.traveller=saves.reference(entity)||'';
  }
  if(input.pressed('ragdoll')&&this.props.rig?.ragdoll){const r=this.props.rig.ragdoll;r.active?r.leave(.5):r.enter();}
  if(!this.state.adopted){const regionTraveller=scenes.resolveRegionEntity('east','2');if(regionTraveller&&scenes.adopt(regionTraveller,'root')){this.state.adopted=saves.reference(regionTraveller);}}
  scenes.setInterest('walker',this.entity.transform.position,{load:16,retain:22});
 }
 fixedUpdate(){if(this.hud.modal)return;const m=this.entity.character,up=m.up;
  const heading=qm(this.entity.transform.rotation,axis({x:0,y:1,z:0},this.state.yaw));
  const forward=rotate(heading,{x:0,y:0,z:-1}),right=rotate(heading,{x:1,y:0,z:0});
  const s=m.state;let velocity=m.velocity;
  if(s.supported){velocity=add(s.supportVelocity,add(mul(forward,input.axis('move_y')*this.props.speed),mul(right,input.axis('move_x')*this.props.speed)));if(input.pressed('jump'))velocity=add(velocity,mul(up,6));}
  m.velocity=velocity;
 }
 projectCamera(){const t=this.entity.presentedTransform;const rotation=qm(qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));
  world.setView({position:add(t.position,rotate(rotation,this.state.third?{x:1,y:2,z:5}:{x:0,y:1.05,z:0})),rotation},70,{near:.15,far:2000});
 }
 presentationUpdate(){this.projectCamera();if(!this.hud)return;
  const projected=world.project({x:0,y:80,z:-800});const marker=this.hud.get('marker');marker.visible=!!projected?.inside;
  if(projected?.inside)marker.setLayout({anchorMin:{x:projected.x,y:projected.y},anchorMax:{x:projected.x,y:projected.y}});
  const p=this.entity.transform.position;this.hud.get('position').text=`${this.state.third?'Third':'First'} person | ${p.x.toFixed(1)}, ${p.y.toFixed(1)}, ${p.z.toFixed(1)} | Traveller ${saves.resolve(this.state.adopted||this.state.traveller)?.id||'none'}`;
 }
 // Opening Escape also produces a UI back event; focus is never activation.
 onUI(event){if(event.type==='back'&&this.openedThisUIFrame)return;if(event.type==='back'||(event.type==='click'&&event.element==='resume')){this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;}if(event.type==='click'&&event.element==='reload')scenes.reload();}
 destroy(){input.pointerCapture=false;world.clearView();}
}
