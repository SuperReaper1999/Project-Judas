import {physics,signals,world,input} from 'judas';
import {game} from './game.js';import {targetOf} from './combat.js';
import {add,mul,norm,tangent,rotate,qm,axis} from './math.js';
const attacks={
 punch:{clip:'Punch',duration:.44,impact:.20,damage:25,speed:1.8,range:1.45,radius:.25},
 heavy:{clip:'HeavyPunch',duration:.8,impact:.50,damage:55,speed:3.5,range:1.55,radius:.3},
 shove:{clip:'Shove',duration:.6,impact:.23,damage:5,speed:6,range:1.65,radius:.36}
};
export default class Melee {
 constructor(player){this.player=player;this.attack=null;this.elapsed=0;this.landed=false;this.serial=0;this.left=world.entity('13');this.right=world.entity('14');this.cue=world.entity('24');}
 get busy(){return !!this.attack;}get clip(){return this.attack?.clip||'';}
 start(kind){if(this.busy||this.player.climb.active||!attacks[kind])return false;
  this.kind=kind;this.attack=attacks[kind];this.elapsed=0;this.landed=false;this.serial++;
  game.message=kind==='heavy'?'HEAVY PUNCH':kind==='shove'?'SHOVE':'PUNCH';return true;
 }
 tick(dt){if(!this.busy){if(input.pressed('shove'))this.start('shove');else if(input.pressed('heavy'))this.start('heavy');else if(input.pressed('punch'))this.start('punch');}
  if(!this.busy)return;this.elapsed+=dt;
  if(!this.landed&&this.elapsed>=this.attack.impact){this.landed=true;this.hit();}
  if(this.elapsed>=this.attack.duration)this.attack=null;
 }
 hit(){const view=this.player.camera.pose(),up=this.player.entity.character.up;
  // Closest swept fist volume wins. Cover is part of the same query, not a
  // distance-only damage check through walls or a global enemy enumeration.
  const origin=add(this.player.entity.transform.position,mul(up,1.25));
  const direction=norm(view.direction),hit=physics.sphereCast(origin,this.attack.radius,direction,this.attack.range,{ignored:[this.player.entity]});
  if(!hit?.entity?.valid){game.message='Missed - get closer';return;}
  const target=targetOf(hit.entity),push=norm(tangent(direction,up));
  if(target){const state=target.scriptState(1);
   if(state&&typeof state==='object'&&!state.dead){signals.send(target,'range.hit',{kind:this.kind,damage:this.attack.damage,direction:push,speed:this.attack.speed,point:hit.point,source:this.player.entity.id});game.flash=this.kind==='heavy' ? .23 : .15;}
   if(hit.entity.id!==target.id&&hit.entity.motionType==='dynamic')hit.entity.applyImpulseAtPoint(mul(push,this.attack.speed*2),hit.point);
  }else if(hit.entity.hasTag('physical')){
   if(hit.entity.motionType==='dynamic')hit.entity.applyImpulseAtPoint(mul(push,this.attack.speed*4),hit.point);
   if(hit.entity.hasTag('destructible'))signals.send(hit.entity,'range.hit',{kind:this.kind,damage:this.attack.damage,direction:push,point:hit.point,source:this.player.entity.id});
  }
  if(this.cue.valid){this.cue.transform={position:hit.point};this.cue.setAudio({pitch:this.kind==='heavy' ? .8 : this.kind==='shove' ? .92 : 1.07});this.cue.playAudioOneShot();}
  const fx=world.entity('20');if(fx.valid){fx.transform={position:hit.point};fx.burst(this.kind==='heavy'?12:5);}
 }
 present(view){const visible=this.busy&&!this.player.camera.third;
  this.left.renderVisible=visible;this.right.renderVisible=visible;if(!visible)return;
  const a=this.attack,t=this.elapsed/a.duration,impact=a.impact/a.duration;
  const extension=t<impact?t/impact:Math.max(0,1-(t-impact)/(1-impact));
  for(const [fist,sign] of [[this.left,-1],[this.right,1]]){
   const striking=this.kind==='shove'||sign===1;
   const reach=striking?extension:extension*.1;
   const offset={x:sign*(this.kind==='heavy'?.24-.12*reach:.24),y:-.28+(this.kind==='heavy'?.08*reach:0),z:-.43-.48*reach};
   fist.transform={position:add(view.eye,rotate(view.rotation,offset)),rotation:qm(view.rotation,axis({x:0,y:0,z:1},sign*(this.kind==='heavy'?.35*reach:.06))),scale:{x:1,y:1,z:1}};
  }
 }
}
