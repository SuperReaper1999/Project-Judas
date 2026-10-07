import {input,ui,world,scenes,saves} from 'judas';
const PREFAB='94da9a1a019e04a190341bf4f0d18212';
export default class {
 constructor(){this.state={spawns:0,clip:0,ik:false,root:false,part:true,mask:false,add:false};}
 start(){this.hud=ui.get('game_ui');this.hud.modal=false;ui.debugOverlayVisible=false;for(const id of ['main','options','pause','image','pause_options'])this.hud.get(id).visible=false;this.hud.get('hud').visible=true;this.hud.get('hud_title').text='M66 ORIGINAL RIG / NORMAL RUNTIME';this.hud.get('hud_help').text='G clips / J mask / K additive / I IK / F ragdoll\nT part / P spawn / B root motor / F5 reload / U stream / F6 save / F7 load\nMouse drag/look, WASD camera / Esc pause';this.a=world.entity('10');this.b=world.entity('11');this.motor=world.entity('12');}
 restore(){this.start();}
 update(){if(!this.a.animation.info.ready)return;
  if(input.pressed('interact')){const names=['Push','Cruise','Walking','PushInPlace'];this.a.animation.crossFade(names[++this.state.clip%names.length],.65);}
  if(input.pressed('pulse')){this.state.mask=!this.state.mask;this.a.animation.layer('arm',{clip:'Walking',mask:['mixamorig:RightArm','mixamorig:RightForeArm','mixamorig:RightHand'],weight:.7,enabled:this.state.mask});}
  if(input.pressed('collect_key')){this.state.add=!this.state.add;this.a.animation.layer('offset',{clip:'Push',referenceClip:'Push',referenceTime:0,additive:true,mask:['mixamorig:Spine'],weight:.25,enabled:this.state.add});}
  if(input.pressed('ik')){this.state.ik=!this.state.ik;this.a.animation.limb('hand',{root:'mixamorig:RightArm',middle:'mixamorig:RightForeArm',end:'mixamorig:RightHand',target:{x:.45,y:1.2,z:.45},pole:{x:1,y:1,z:0},weight:.65,enabled:this.state.ik});}
  if(input.pressed('ragdoll_primary')){if(this.a.ragdoll.active)this.a.ragdoll.leave(.7);else this.a.ragdoll.enter();}
  if(input.pressed('part')){this.state.part=!this.state.part;const part=this.a.modelParts[0];this.a.setPartVisible(part.identity,this.state.part);}
  if(input.pressed('spawn_prefab')){world.spawnPrefab(PREFAB,{position:{x:-4+(this.state.spawns++%5)*2,y:2,z:-3}});}
  if(input.pressed('root_intent'))this.state.root=!this.state.root;
  if(input.pressed('reload_scene'))scenes.reload();
  if(input.pressed('save_state'))saves.save('import-lab');
  if(input.pressed('delete_state'))saves.load('import-lab');
  if(input.pressed('stream')){if(this.region){scenes.releaseRegion(this.region);scenes.unloadRegion('rig');this.region=null;}else this.region=scenes.requestRegion('rig');}
  this.hud.get('counter').text=`65 source bones / 6 parts / independent instances
${this.a.animation.info.clip} ${this.a.animation.time.toFixed(2)}s / ragdoll ${this.a.ragdoll.active}
IK ${this.state.ik} / root motor ${this.state.root} / spawned ${this.state.spawns}`;
 }
 fixedUpdate(dt){if(!this.state.root||!this.motor.animation.info.ready)return;const t=this.motor.animation.time,m=this.motor.animation.rootMotion('WalkingExtract',t,t+dt,true);const v={x:m.translation.x/dt,y:this.motor.character.velocity.y,z:m.translation.z/dt};this.motor.character.velocity=v;}
 uiUpdate(){this.pauseEdge=input.pressed('pause');if(this.pauseEdge){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;}}
 onUI(e){if(e.type==='back'&&!this.pauseEdge||e.type==='click'&&e.element==='resume'){this.hud.modal=false;this.hud.get('pause').visible=false;}if(e.type==='click'&&e.element==='pause_quit')ui.quit();}
}
