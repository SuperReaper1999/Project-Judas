import {physics,world,signals,input} from 'judas';
import {game} from './game.js';import {targetOf} from './combat.js';import {launchRocket} from './explosives.js';
import {add,sub,mul,norm,dot,length,tangent,rotate,qm,axis} from './math.js';
const CAPACITY=24,RELOAD_SECONDS=1.25;
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
export default class Weapon {
 constructor(player){this.player=player;this.cooldown=0;this.ammo=CAPACITY;this.reload=0;this.age=0;this.shotIndex=0;this.quiet=1;
  this.impact=world.entity('20');this.flash=world.entity('21');this.reloadCue=world.entity('23');this.emptyCue=world.entity('25');
  this.mode='rifle';this.launcher=world.entity('15');this.aimBlend=0;this.kick=0;this.recoilRecover=0;this.reloadStage='';this.reloadClick=false;
 }
 get capacity(){return CAPACITY;}get selectedAmmo(){return this.mode==='rpg'?game.rockets:this.ammo;}get selectedCapacity(){return this.mode==='rpg'?null:CAPACITY;}get aimed(){return this.aimBlend>.6;}get lookScale(){return 1-this.aimBlend*.38;}
 beginReload(){if(this.mode!=='rifle'||this.reload>0||this.ammo===CAPACITY)return false;
  this.reload=RELOAD_SECONDS;this.reloadClick=false;this.reloadStage='MAGAZINE';game.message='RELOADING - find cover';
  if(this.reloadCue.valid)this.reloadCue.playAudioOneShot();return true;
 }
 select(mode){if(mode===this.mode)return;this.mode=mode;this.reload=0;this.reloadStage='';this.aimBlend=0;game.message=mode==='rpg'?'ROCKET LAUNCHER - mind the blast radius':'RIFLE';}
 tick(dt){if(input.pressed('weapon_rifle'))this.select('rifle');if(input.pressed('weapon_launcher'))this.select('rpg');this.age+=dt;this.quiet+=dt;this.cooldown=Math.max(0,this.cooldown-dt);this.kick*=Math.exp(-14*dt);
  const requested=input.held('aim')&&!this.player.melee?.busy&&!this.player.climb.active&&this.reload<=0;
  this.aimBlend+=clamp((requested?1:0)-this.aimBlend,-8*dt,8*dt);
  // Only part of each shot's small aim kick recovers automatically. Looking
  // remains player input; sustained fire still benefits from active control.
  const recovery=this.recoilRecover*(1-Math.exp(-10*dt));this.recoilRecover-=recovery;
  this.player.state.pitch=clamp(this.player.state.pitch-recovery,-1.4,1.4);
  if(this.reload>0){this.reload=Math.max(0,this.reload-dt);const progress=1-this.reload/RELOAD_SECONDS;
   this.reloadStage=progress<.47?'MAGAZINE':progress<.82?'SEAT MAGAZINE':'CHARGE';
   if(progress>.66&&!this.reloadClick){this.reloadClick=true;if(this.reloadCue.valid)this.reloadCue.playAudioOneShot();}
   if(this.reload===0){this.ammo=CAPACITY;this.reloadStage='';game.message='RIFLE READY';}
  }
  if(input.pressed('reload'))this.beginReload();
 }
 fire(){if(this.cooldown>0||this.reload>0)return false;
  if(this.mode==='rpg'){if(!launchRocket(this.player,this.player.camera.pose())){this.cooldown=.35;if(this.emptyCue.valid)this.emptyCue.playAudioOneShot();return false;}
   this.cooldown=.9;this.kick=1.6;this.player.state.pitch=clamp(this.player.state.pitch+.05,-1.4,1.4);this.recoilRecover+=.018;return true;}

  if(this.ammo===0){if(this.emptyCue.valid)this.emptyCue.playAudioOneShot();this.beginReload();return false;}
  this.cooldown=.125;this.ammo--;game.shots++;this.shotIndex++;
  const view=this.player.camera.pose(),motor=this.player.entity.character,up=motor.up;
  const moving=clamp(length(tangent(motor.velocity,up))/6,0,1),first=this.quiet>.3;this.quiet=0;
  // A bounded, deterministic cone in the camera basis; the aimed first shot is
  // precise. Both casts use this one direction, preserving third-person cover.
  const spread=first?0:(.006*(1-this.aimBlend)+.0015*this.aimBlend)*(1+moving);
  const phase=this.shotIndex*2.3999632297,radius=spread*Math.sqrt((this.shotIndex*.61803398875)%1);
  const aim=norm(add(view.direction,rotate(view.rotation,{x:Math.cos(phase)*radius,y:Math.sin(phase)*radius,z:0})));
  const filter={ignored:[this.player.entity],includeSensors:false};
  const sight=physics.raycast(view.position,aim,110,filter),target=sight?sight.point:add(view.position,mul(aim,110));
  const direction=norm(sub(target,view.eye)),hit=physics.raycast(view.eye,direction,110,filter);
  game.noiseTime=.9;game.noisePosition=view.eye;
  if(hit){
   if(this.impact.valid){this.impact.transform={position:hit.point};this.impact.burst(12);this.impact.playAudioOneShot();}
   if(hit.entity?.valid){const victim=targetOf(hit.entity);
    if(victim){
     const state=victim.scriptState(1),alive=state&&typeof state==='object'&&!state.dead;
     if(alive){
      const localUp=victim.character?.up||up;
      let head=dot(sub(hit.point,victim.transform.position),localUp)>1.43;
      // Fallen skeletons no longer have an upright height threshold. Consult
      // their actual mapped head, while ownership still comes from targetOf.
      const visual=victim.children.find(e=>e.valid&&e.ragdoll);
      if(visual?.ragdoll?.active){const skull=visual.ragdoll.body('head');head=!!skull?.valid&&length(sub(hit.point,skull.transform.position))<.25;}
      signals.send(victim,'range.hit',{kind:'bullet',damage:head?80:28,direction,point:hit.point,source:this.player.entity.id});
      game.hits++;game.flash=head ? .23 : .12;game.message=head?'HEADSHOT!':'HIT';
     }
     if(victim.id!==hit.entity.id&&hit.entity.motionType==='dynamic')hit.entity.applyImpulseAtPoint(mul(direction,6),hit.point);
    }else if(hit.entity.hasTag('physical')){
     if(hit.entity.motionType==='dynamic')hit.entity.applyImpulseAtPoint(mul(direction,8),hit.point);
     if(hit.entity.hasTag('destructible'))signals.send(hit.entity,'range.hit',{kind:'bullet',damage:28,direction,point:hit.point,source:this.player.entity.id});
    }
   }
  }
  if(this.flash.valid){this.flash.transform={position:add(view.eye,mul(direction,.55)),rotation:view.rotation};this.flash.burst(4);}
  this.player.entity.setAudio({pitch:.95+.1*((this.shotIndex*.37)%1)});this.player.entity.playAudioOneShot();
  const recoil=(.027-.013*this.aimBlend)*(motor.supported?1:1.2);
  this.player.state.pitch=clamp(this.player.state.pitch+recoil,-1.4,1.4);this.recoilRecover+=recoil*.45;
  this.player.state.yaw+=Math.sin(this.shotIndex*1.7)*(.004-.002*this.aimBlend);this.kick=Math.min(1.7,this.kick+1);
  if(this.ammo===0)game.message='EMPTY - RELOAD';return true;
 }
 present(view){const rifle=this.player.gun,gun=this.mode==='rpg'?this.launcher:rifle;if(rifle?.valid)rifle.renderVisible=false;if(this.launcher.valid)this.launcher.renderVisible=false;if(!gun?.valid)return;
  gun.renderVisible=!this.player.melee?.busy&&!this.player.climb.active;if(!gun.renderVisible)return;
  const third=this.player.camera.third,aim=third?0:this.aimBlend;
  const motion=clamp(length(tangent(this.player.entity.character.velocity,this.player.entity.character.up))/5,0,1);
  const bob=Math.sin(this.age*9)*.009*motion*(1-aim*.8),sway=Math.cos(this.age*4.5)*.009*motion*(1-aim*.8);
  const progress=this.reload>0?1-this.reload/RELOAD_SECONDS:0,lower=this.reload>0?Math.sin(Math.PI*progress):0;
  const launcher=this.mode==='rpg';
  const offset=third?{x:.27,y:-.50,z:-.40}:{x:.23-.20*aim+sway,y:-.27+.06*aim+bob-.20*lower,z:-.48+.05*this.kick+.06*lower};
  if(launcher){offset.x+=.1;offset.y-=.03;offset.z+=.13;}
  const tilt=axis({x:0,y:0,z:1},-.40*lower),kick=axis({x:1,y:0,z:0},.08*this.kick+.22*lower);
  gun.transform={position:add(view.eye,rotate(view.rotation,offset)),rotation:qm(qm(qm(view.rotation,tilt),kick),axis({x:0,y:1,z:0},Math.PI)),scale:launcher?{x:1,y:1,z:1}:{x:.55,y:.55,z:.55}};
 }
}
