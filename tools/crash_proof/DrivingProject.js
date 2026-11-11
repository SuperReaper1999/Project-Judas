// Project-owned crash-yard controls. The author prefixes the actual `rigs` layout.
// No damage is scripted: launch supplies one physical impulse to connected mass.
const rotate=(q,v)=>{const x=2*(q.y*v.z-q.z*v.y),y=2*(q.z*v.x-q.x*v.z),z=2*(q.x*v.y-q.y*v.x);return {x:v.x+q.w*x+q.y*z-q.z*y,y:v.y+q.w*y+q.z*x-q.x*z,z:v.z+q.w*z+q.x*y-q.y*x};};
const multiply=(a,b)=>({w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w});
const rotation=(yaw,pitch)=>{const cy=Math.cos(yaw/2),sy=Math.sin(yaw/2),cp=Math.cos(pitch/2),sp=Math.sin(pitch/2);return {w:cy*cp,x:cy*sp,y:sy*cp,z:-sy*sp};};
const launchSpeeds=[2,4,8,15,25,35];
export default class {
 constructor(){this.state={selected:0,yaw:0,pitch:-.28,distance:10,speedIndex:4};this.request=null;this.focus=null;this.pendingBehind=true;this.observeTicks=0;}
 start(){this.state.selected=Math.min(rigs.length-1,Math.max(0,this.state.selected||0));this.state.yaw??=0;this.state.pitch??=-.28;this.state.distance??=10;this.state.speedIndex??=4;this.focus=null;this.pendingBehind=true;input.pointerCapture=false;}
 restore(){this.start();}
 report(kind,extra={}){console.log('CRASH_YARD_EVENT '+JSON.stringify({kind,selected:this.state.selected+1,owner:rigs[this.state.selected].owner,...extra}));}
 current(){const rig=rigs[this.state.selected],owner=world.entity(rig.owner),frame=owner.fracture?.state;const ref=frame?.parts.find(p=>p.index===rig.reference&&!p.removed);return {rig,owner,frame,ref};}
 uiUpdate(){
  const hud=ui.get('crash-project');
  if(input.pressed('pause')&&hud)hud.modal=!hud.modal;
  const paused=!!hud?.modal;input.pointerCapture=!paused&&input.held('orbit');
  if(!paused){
   for(let n=0;n<rigs.length;n++)if(input.pressed('select_'+(n+1))){this.state.selected=n;this.focus=null;this.pendingBehind=true;this.report('select');}
   if(input.pressed('speed')){this.state.speedIndex=(this.state.speedIndex+1)%launchSpeeds.length;this.report('speed',{metresPerSecond:launchSpeeds[this.state.speedIndex]});}
   if(input.pressed('behind'))this.pendingBehind=true;
   if(input.held('orbit')){const x=input.axis('look_x'),y=input.axis('look_y');this.state.yaw-=x*.004;this.state.pitch=Math.max(-1.15,Math.min(.15,this.state.pitch-y*.004));if(x||y)this.reportView=true;}
   this.state.distance=Math.max(4,Math.min(24,this.state.distance-input.axis('zoom')));
   if(input.pressed('reset'))scenes.reload();
  }
  if(!saves.busy){if(input.pressed('save'))this.request=saves.save('crash-yard');if(input.pressed('load'))this.request=saves.load('crash-yard');}
  if(this.request){const status=saves.status(this.request);if(status.state!==this.lastSaveStatus){this.lastSaveStatus=status.state;this.report('save-status',{state:status.state,error:status.error||''});}}
  if(!hud)return;
  const {rig,frame,ref}=this.current();const velocity=ref?.entity?ref.entity.velocity:{x:0,z:0};
  const speed=Math.hypot(velocity.x,velocity.z)*3.6;const target=launchSpeeds[this.state.speedIndex];
  hud.get('telemetry').text='CRASH YARD | CAR '+(this.state.selected+1)+' / '+rigs.length+' | '+(rig.label||'Challenger')+' | '+speed.toFixed(0)+' km/h\n'
   +'W/S drive / reverse | A/D steer | Space brake | '+(rigs.length>1?'1-'+rigs.length+' select car':'One authored car')+'\n'
   +'Hold RMB + mouse: orbit | Wheel: zoom | C: behind car | R: reset yard\n'
   +'Enter: launch to '+(target*3.6).toFixed(0)+' km/h ('+target+' m/s) | X: cycle launch speed\n'
   +'K/L save/load | Esc pause | '+(rig.target?'Lane target: '+rig.target:'Damage follows physical contact')+'\n'
   +(frame?'Broken connections: '+frame.interfaces.filter(i=>i.broken).length+' / '+frame.interfaces.length:'Preparing structure')
   +(this.request?' | Save: '+saves.status(this.request).state+(saves.status(this.request).error?' - '+saves.status(this.request).error:''):'')+(paused?' | PAUSED':'');
 }
 launch(){
  const {rig,owner,frame,ref}=this.current();if(!ref?.entity)return;
  const body=ref.entity,direction=rotate(body.transform.rotation,rig.forward||{x:0,y:0,z:-1});const length=Math.hypot(direction.x,direction.z);if(length<1e-6)return;
  direction.x/=length;direction.y=0;direction.z/=length;
  const speed=launchSpeeds[this.state.speedIndex],velocity=body.velocity;
  const delta=Math.max(0,speed-velocity.x*direction.x-velocity.z*direction.z);let mass=0,parts=0;
  for(const p of frame.parts)if(!p.removed&&p.component===ref.component){owner.fracture.impulse(p.index,{x:direction.x*delta*p.mass,y:0,z:direction.z*delta*p.mass});mass+=p.mass;parts++;}
  for(const id of rig.panels||[]){const panel=world.entity(id).deformable,state=panel?.state;if(!state)continue;
   const connected=state.attachments.some(a=>a.active&&!a.released&&frame.parts.some(p=>p.key===a.part&&p.component===ref.component));
   if(connected)panel.impulse({x:direction.x*delta*state.mass,y:0,z:direction.z*delta*state.mass});
  }
  this.observeTicks=180;this.report('launch',{speed,delta,mass,parts});
 }
 fixedUpdate(){
  if(input.pressed('launch'))this.launch();
  const driving=Math.abs(input.axis('drive'))>.01;if(driving&&!this.wasDriving)this.observeTicks=300;this.wasDriving=driving;
  for(let n=0;n<rigs.length;n++){const rig=rigs[n],active=n===this.state.selected,owner=world.entity(rig.owner),power=owner.powertrain;
   if(power)power.set({sources:power.settings.sources.map(source=>({name:source.name,input:active?input.axis('drive'):0})),brakes:power.settings.brakes.map(b=>({name:b.name,engagement:!active||input.held('brake')?1:0}))});
   for(let w=0;w<rig.wheels.length;w++){const wheel=world.entity(rig.wheels[w]).wheel;if(!wheel)continue;wheel.set({steeringAngle:active&&(rig.steered?rig.steered.includes(w):w<2)?input.axis('steer')*.4:0});
    if(rig.brakes[w]){const state=wheel.state,pose=world.entity(rig.brakes[w]).transform;pose.position=state.center;pose.rotation=multiply(state.rotation,{w:Math.cos(state.angle/2),x:Math.sin(state.angle/2),y:0,z:0});world.entity(rig.brakes[w]).transform=pose;}
   }
  }
  if(this.observeTicks>0&&--this.observeTicks%30===0){const {frame,ref}=this.current();this.report('damage',{speed:ref?.entity?Math.hypot(...['x','z'].map(k=>ref.entity.velocity[k])):0,broken:frame?.interfaces.filter(i=>i.broken).length,position:ref?.entity?ref.entity.transform.position:null,panels:(rigs[this.state.selected].panels||[]).map(id=>{const s=world.entity(id).deformable?.state;return s?{id,bend:s.panel?.maximumPlasticAngle,released:s.attachments.map(a=>a.released),work:s.attachments.map(a=>a.plasticWork)}:null;})});}
 }
 presentationUpdate(dt){
  const {ref}=this.current();if(!ref?.entity)return;const pose=ref.entity.presentedTransform;
  if(this.pendingBehind){const direction=rotate(pose.rotation,rigs[this.state.selected].forward||{x:0,y:0,z:-1});this.state.yaw=Math.atan2(-direction.x,-direction.z);this.pendingBehind=false;}
  const goal={x:pose.position.x,y:pose.position.y+.45,z:pose.position.z};
  if(!this.focus)this.focus={...goal};
  const h=1-Math.exp(-8*Math.max(0,dt)),v=1-Math.exp(-3*Math.max(0,dt));
  this.focus={x:this.focus.x+(goal.x-this.focus.x)*h,y:this.focus.y+(goal.y-this.focus.y)*v,z:this.focus.z+(goal.z-this.focus.z)*h};
  const q=rotation(this.state.yaw,this.state.pitch),back=rotate(q,{x:0,y:0,z:this.state.distance});
  const eye={x:this.focus.x+back.x,y:Math.max(.3,this.focus.y+back.y),z:this.focus.z+back.z};
  world.setView({position:eye,rotation:q},65,{near:.1,far:500});
  if(this.reportView){this.report('camera',{yaw:this.state.yaw,pitch:this.state.pitch,eye,focus:this.focus});this.reportView=false;}
 }
 onOriginShift(){this.focus=null;}
 destroy(){input.pointerCapture=false;world.clearView();}
}
