import {entity,input,world,physics,ui,saves,scenes,console} from 'judas';
import FirstPerson,{add,mul,qm,axis,rotate} from './player.js';
export {properties} from './player.js';
export default class NightWatch extends FirstPerson {
 constructor(context){super(context);Object.assign(this.state,{time:0,day:false,torch:true,patrol:true,emergency:true,broken:{},request:0});}
 start(){super.start();this.bind();}
 restore(){super.start();this.bind();console.log('LUCIFER_RESTORED '+JSON.stringify({time:this.state.time,broken:this.state.broken,day:this.state.day}));}
 bind(){this.hud=ui.get('night');if(this.hud)this.hud.modal=false;this.flashlight=entity(100);this.status='Aim at a bulb and shoot. Lamps stay broken after cold save/load.';this.applyBroken();for(let n=0;n<2;++n)entity(522+n).material().set({emissive:{x:2,y:1.8,z:1.2}});}
 applyBroken(){for(let n=0;n<4;++n){const lamp=entity(200+n);if(lamp?.valid){const broken=!!this.state.broken[String(lamp.id)];lamp.setLight({enabled:!broken});lamp.material().set({emissive:broken?{x:0,y:0,z:0}:{x:2,y:1.2,z:.35}});}}}
 uiUpdate(){super.uiUpdate();if(input.pressed('torch'))this.state.torch=!this.state.torch;if(input.pressed('patrol'))this.state.patrol=!this.state.patrol;if(input.pressed('emergency'))this.state.emergency=!this.state.emergency;
  if(input.pressed('save'))this.state.request=saves.save('night-watch',{name:'Night Watch'});
  if(input.pressed('load'))this.state.request=saves.load('night-watch');
  if(input.pressed('reset'))scenes.reload();
  if(input.pressed('day')){this.state.day=!this.state.day;world.setAppearance({sunEnabled:this.state.day,ambientColor:this.state.day?{x:.11,y:.12,z:.15}:{x:.015,y:.02,z:.035},backgroundColor:this.state.day?{x:.25,y:.4,z:.65}:{x:.008,y:.012,z:.025}});}
  if(this.hud){let text=this.status;if(this.state.request){const op=saves.status(this.state.request);if(op){text+='  Save/load: '+op.state+(op.error?' '+op.error:'');if(op.state!==this.saveState){this.saveState=op.state;console.log('LUCIFER_SAVE '+JSON.stringify(op));}}}this.hud.get('status').text=text;
   const sample=this.state.exposure;this.hud.get('exposure').text=sample?('Exposure '+sample.relativeIlluminance.toFixed(3)+' relative | '+(sample.complete?'complete':'INCOMPLETE: '+sample.unresolvedSources+' unknown')+' | sampled '+Math.max(0,this.state.time-this.state.exposureTime).toFixed(1)+' s ago'): 'Exposure: waiting for physical sample';}

 }
 update(dt){super.update(dt);this.state.time+=dt;
  if(this.state.time>=(this.nextExposure||0)){this.nextExposure=this.state.time+.25;this.state.exposure=world.lightExposure(this.ray().position,{ignored:[this.entity],maxOcclusionQueries:32});this.state.exposureTime=this.state.time;}

  if(input.pressed('fire')&&this.state.captured){const ray=this.ray();const hit=physics.raycast(ray.position,rotate(ray.rotation,{x:0,y:0,z:-1}),45,{ignored:[this.entity]});
   if(hit?.entity&&Number(hit.entity.id)>=200&&Number(hit.entity.id)<204){this.state.broken[String(hit.entity.id)]=true;this.applyBroken();this.status='Bulb destroyed: illumination disabled without rebuilding the scene.';console.log('LUCIFER_LAMP_DESTROYED',String(hit.entity.id));}
   else this.status='Shot missed a bulb. Aim directly at the warm hanging light.';
  }
 }
 fixedUpdate(dt){super.fixedUpdate(dt);const t=this.state.time;
  for(let n=0;n<8;++n){const actor=entity(300+n*10);if(!actor?.valid)continue;const a=t*.22+n*.65;const p={x:-5+n%4*4+Math.sin(a)*1.5,y:.95,z:5-Math.floor(n/4)*11+Math.cos(a)*1.5};actor.moveKinematic({position:p,rotation:axis({x:0,y:1,z:0},a)},dt);
   for(let j=2;j<6;++j){const limb=entity(302+n*10+j-2);const parent=actor.transform;const local=j<4?{x:j===2?-.37:.37,y:-.04,z:0}:{x:j===4?-.14:.14,y:-.6,z:0};const pose=limb.transform;pose.position=add(parent.position,rotate(parent.rotation,local));pose.rotation=qm(parent.rotation,axis({x:1,y:0,z:0},Math.sin(t*4+n+(j%2)*Math.PI)*.3));limb.transform=pose;}
  }
  const car=entity(500);if(this.state.patrol&&car?.valid){const a=t*.10;car.moveKinematic({position:{x:7+Math.sin(a)*3,y:.68,z:-7+Math.cos(a)*8},rotation:axis({x:0,y:1,z:0},Math.atan2(-3*Math.cos(a),8*Math.sin(a)))},dt);}else if(car?.valid)car.stopKinematic();
  for(let n=0;n<2;++n){const e=entity(530+n);if(e?.valid)e.setLight({enabled:this.state.emergency&&Math.floor(t*6)%2===n});const lens=entity(532+n);if(lens?.valid)lens.material().set({emissive:this.state.emergency&&Math.floor(t*6)%2===n?(n===0?{x:3,y:.05,z:.03}:{x:.04,y:.4,z:3}):{x:0,y:0,z:0}});}
 }
 ray(){const pose=this.entity.presentedTransform;return {position:add(pose.position,rotate(pose.rotation,{x:0,y:.65,z:0})),rotation:qm(qm(pose.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch))};}
 presentationUpdate(){super.presentationUpdate();const ray=this.ray();if(this.flashlight?.valid){const p=this.flashlight.transform;p.position=add(ray.position,rotate(ray.rotation,{x:.15,y:-.12,z:-.15}));p.rotation=ray.rotation;this.flashlight.transform=p;this.flashlight.setLight({enabled:this.state.torch});}}
}
