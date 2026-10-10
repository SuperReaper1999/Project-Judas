import {world,physics,signals,console} from 'judas';import {game} from './game.js';
import {activeTrips,registerBones,registerCorpse,retireBones,tripBones} from './combat.js';
import {add,sub,mul,dot,length,norm,axis,tangent} from './math.js';
import ZombieAI from './zombie_ai.js';
export const properties={kind:{type:'string',default:'zombie'},variant:{type:'string',default:'auto'}};
const angleDifference=(a,b)=>Math.atan2(Math.sin(b-a),Math.cos(b-a));
const clipFallback={ZombieIdle:'Idle',Shamble:'Walk',ZombieRun:'Run',Attack:'Idle',Stagger:'Idle'};
export default class Enemy {
 constructor({entity,properties}){this.entity=entity;this.props=properties;this.state={health:properties.kind==='soldier'?50:75,dead:false,age:0,tripped:false,shoves:0,trips:0,variant:'',awareness:'search',attackPhase:''};this.next=0;this.attack=1.5;this.clip='';this.stun=0;this.knock={x:0,y:0,z:0};this.pushTime=0;this.tripTime=0;this.retry=0;this.animClock=1;this.animRead=0;this.locomoting=false;this.forceTrip=false;}
 start(){
  this.player=world.entity('10');this.token=signals.subscribe('range.hit');this.soldier=this.props.kind==='soldier';this.visual=this.entity.children[0];this.yaw=0;this.previousYaw=0;
  if(!this.soldier){this.brain=new ZombieAI(this.entity,this.player,this.props.variant);this.state.health=this.brain.type.health;this.state.variant=this.brain.variant;}
  this.entity.navigation.configure({speed:this.soldier?2.2:this.brain.type.speed});
 }
 onSignal(e){if(e.name!=='range.hit'||this.state.dead||game.phase!=='wave')return;
  const hit=e.payload;if(typeof hit?.damage!=='number'||!Number.isFinite(hit.damage)||hit.damage<=0)return;
  this.state.health-=hit.damage;
  if(this.brain){this.brain.alert(this.player.transform.position,true);this.brain.clearAttack();}
  if(hit.kind==='shove'||hit.kind==='heavy'||hit.kind==='punch'||hit.kind==='blast'){
   this.stun=Math.max(this.stun,hit.kind==='blast' ? .8 : hit.kind==='heavy' ? .65 : hit.kind==='shove' ? .75 : .22);
   this.attack=Math.max(this.attack,this.stun+.3);
   if(hit.direction&&hit.speed&&!this.state.tripped){this.knock=mul(norm(hit.direction),hit.speed*(this.brain?.type.shoveScale||1));this.pushTime=hit.kind==='shove' ? .65 : hit.kind==='blast' ? .55 : .25;}
   if(hit.kind==='blast'&&hit.speed>=5&&!this.soldier)this.forceTrip=true;
   if(hit.kind==='shove')this.state.shoves++;
   game.message=hit.kind==='shove'?'SHOVED - push them into low cover to trip!':hit.kind==='heavy'?'HEAVY HIT':hit.kind==='blast'?'BLAST HIT':'PUNCH HIT';
  }else if(this.brain&&this.brain.variant!=='brute'){
   this.stun=Math.max(this.stun,.13);
  }
  if(this.state.health<=0){this.state.dead=true;this.entity.character.enabled=false;this.entity.navigation.clear();this.entity.navigation.enabled=false;
   // Animation is presentation. A rejected clip/fade must never prevent death
   // accounting or disable this script's subsequent damage callbacks.
   try{this.visual.animation?.pause();}catch(error){this.animationWarning(error);}
   const reward=this.soldier?150:this.brain.variant==='brute'?175:this.brain.variant==='stalker'?125:100;
   game.alive--;game.kills++;game.score+=reward;game.points=(game.points||0)+reward;
   game.message=`${this.soldier?'SOLDIER':this.brain.variant.toUpperCase()} DOWN +${reward}`;
   for(const c of this.visual.children)if(c.valid)c.renderVisible=false;
   // Activate at the next authoritative script step, from the current resolved
   // pose. A trip already has physical authority and stays physical on death.
   this.deathHit=hit;this.deathPending=true;this.corpseAge=0;
  }
 }
 diePhysical(){const r=this.visual?.valid?this.visual.ragdoll:null;if(!r||!this.visual.animation?.info.ready)return;
  const alreadyActive=r.active;
  if(!alreadyActive){
   this.visual.transform={position:this.entity.transform.position,rotation:axis({x:0,y:1,z:0},this.yaw)};
   this.visual.animation.pause();r.enter();
   // Judas already inherits recent world/bone motion when entering. Add only
   // this game's killing impact; do not add motor velocity a second time.
   const hit=this.deathHit,bodies=tripBones.map(k=>r.body(k)).filter(b=>b?.valid);
   if(hit?.direction&&hit.kind==='blast'){
    for(const body of bodies)body.applyImpulse(mul(norm(hit.direction),body.mass*Math.max(0,hit.speed||0)));
   }else if(hit?.direction&&hit.point){
    const nearest=bodies.sort((a,b)=>length(sub(a.transform.position,hit.point))-length(sub(b.transform.position,hit.point)))[0];
    const magnitude=hit.kind==='bullet'?6:Math.max(2,(hit.speed||1)*2);
    if(nearest)nearest.applyImpulseAtPoint(mul(norm(hit.direction),magnitude),hit.point);
   }
  }
  registerBones(this.entity,r);registerCorpse(this.entity);this.state.ragdoll=true;
  this.state.tripped=false;this.deathPending=false;this.deathHit=null;this.pushTime=0;this.returnTime=0;this.brain?.clearAttack();
 }
 trip(){const ragdoll=this.visual.ragdoll;if(this.soldier||!ragdoll||this.state.tripped||!this.visual.animation?.info.ready||activeTrips.size>=6)return false;
  const t=this.entity.transform;this.visual.transform={position:t.position,rotation:axis({x:0,y:1,z:0},this.yaw)};
  this.entity.character.enabled=false;this.entity.navigation.stopped=true;this.visual.animation.pause();
  try{ragdoll.enter();}catch(error){
   this.entity.character.enabled=true;this.entity.navigation.stopped=false;this.visual.animation.resume();
   console.log(`Trip articulation unavailable: ${error.message}`);return false;
  }
  activeTrips.add(this.entity.id);registerBones(this.entity,ragdoll);
  // One game-authored shove impulse. Afterward ordinary gravity, contacts and
  // passive articulation own the entire fall; no pose drive or fake forces.
  const chest=ragdoll.body('chest');if(chest?.valid)chest.applyImpulse(mul(norm(this.knock),10));
  this.state.tripped=true;this.state.trips++;this.tripTime=2.1;this.retry=0;this.pushTime=0;this.brain?.clearAttack();game.message='ZOMBIE TRIPPED!';return true;
 }
 placement(){const r=this.visual.ragdoll,bodies=tripBones.map(k=>r.body(k)).filter(b=>b?.valid);
  const ignored=[this.entity,...bodies,...world.queryTags(['enemy'])],filter={ignored};
  const center=this.visual.transform.position,u=this.entity.character.up;
  const pelvis=r.body('pelvis'),origin=pelvis?.valid?pelvis.transform.position:add(center,mul(u,1));
  for(const offset of [{x:0,y:0,z:0},{x:.65,y:0,z:0},{x:-.65,y:0,z:0},{x:0,y:0,z:.65},{x:0,y:0,z:-.65}]){
   const above=add(add(center,offset),mul(u,2));const ground=physics.raycast(above,mul(u,-1),4,filter);
   if(!ground||dot(ground.normal,u)<.65)continue;
   const position=add(ground.point,mul(u,.04)),capsule=add(position,mul(u,.9));
   const occupied=physics.capsuleCast({position:capsule,rotation:this.entity.transform.rotation},.3,.6,u,0,filter);if(occupied)continue;
   const to=sub(capsule,origin),distance=length(to);if(distance>.01&&physics.raycast(origin,norm(to),distance,filter))continue;
   return position;
  }
  return null;
 }
 recover(){const r=this.visual.ragdoll;
  if(this.state.dead)return false;
  const position=this.placement();if(!position)return false;
  const previous=this.visual.transform.position;
  r.leave(.45);retireBones(this.entity);this.entity.transform={position};
  // The captured fallen pose uses its old visual origin. Bring that origin
  // toward the safe standing placement during the same visual return fade.
  this.returnOffset=sub(previous,position);this.returnTime=.45;
  this.visual.transform={position:previous,rotation:axis({x:0,y:1,z:0},this.yaw)};
  this.entity.character.enabled=true;this.entity.character.velocity={x:0,y:0,z:0};
  this.entity.navigation.stopped=false;this.visual.animation.resume();this.state.tripped=false;this.pushTime=0;this.knock={x:0,y:0,z:0};this.stun=.45;this.next=0;this.clip='';this.animClock=1;this.attack=1;this.forceTrip=false;
  if(this.brain){this.brain.clearAttack();this.brain.repath=0;}return true;
 }
 shouldTrip(dt){if(this.soldier||this.state.tripped||this.pushTime<=0)return false;
  const m=this.entity.character,u=m.up,p=this.entity.transform.position,dir=norm(tangent(this.knock,u));if(length(dir)<.1)return false;
  const filter={ignored:[this.entity]},distance=Math.max(.42,length(this.knock)*dt+.12);
  const low=physics.raycast(add(p,mul(u,.22)),dir,distance,filter);
  if(low&&dot(low.normal,u)<.6){const high=physics.raycast(add(p,mul(u,1.45)),dir,distance,filter);
   if(!high||high.distance>low.distance+.12)return true;}
  if(!m.supported){const ahead=add(p,mul(dir,.35));const floor=physics.raycast(add(ahead,mul(u,.1)),mul(u,-1),.65,filter);if(!floor)return true;}
  return false;
 }
 animationWarning(error){if(this.animationFaultLogged)return;this.animationFaultLogged=true;console.log(`Enemy animation presentation fallback (${this.entity.id}): ${error.message}`);}
 animate(wanted,speed,dt,loop=true){
  this.animClock+=dt;this.animRead-=dt;
  const a=this.visual?.valid?this.visual.animation:null;if(!a)return;
  try{
   if(!this.clips&&this.animRead<=0){this.animRead=.3;const info=a.info;if(info.ready)this.clips=new Set(info.clips.map(c=>c.name));}
   if(!this.clips)return;
   const available=this.clips.has(wanted)?wanted:clipFallback[wanted]||'Idle';
   // The old per-frame Idle/Run threshold could interrupt every crossfade and
   // exhaust the documented 16-source bound. Hysteresis and a minimum dwell
   // longer than the fade allow completed contributors to retire normally.
   if(available!==this.clip&&this.animClock>=.28){a.loop=loop;a.crossFade(available,.12);this.clip=available;this.animClock=0;}
   a.speed=Math.max(.55,Math.min(1.65,speed));
  }catch(error){
   this.animationWarning(error);
   // Documented immediate playback clears an interrupted mixer if a stale
   // presentation state fails. Damage/motion remain live; no native budget is
   // increased, and ordinary transitions use the bounded dwell above.
   try{const fallback=this.clips?.has(wanted)?wanted:clipFallback[wanted]||'Idle';a.loop=true;a.play(fallback);this.clip=fallback;this.animClock=0;}catch(ignored){}
  }
 }
 turn(direction,dt){if(!direction||length(direction)<.1)return;
  const wanted=Math.atan2(direction.x,direction.z),delta=angleDifference(this.yaw,wanted);
  const rate=this.soldier?6:this.brain.variant==='brute'?3.2:6;
  this.yaw+=Math.sign(delta)*Math.min(Math.abs(delta),rate*dt);
 }
 strike(result){if(!result)return;
  if(result.obstacle?.valid){
   const body=result.obstacle;
   if(body.hasTag('destructible'))signals.send(body,'range.hit',{kind:'zombie',damage:this.brain.variant==='brute'?42:23,direction:result.direction,point:result.point});
   if(body.motionType==='dynamic')body.applyImpulseAtPoint(mul(result.direction,this.brain.variant==='brute'?13:7),result.point);
  }else if(result.damage)signals.send(this.player,'player.hurt',{damage:result.damage});
 }
 fixedUpdate(dt){this.state.age+=dt;this.previousYaw=this.yaw;this.returnTime=Math.max(0,(this.returnTime||0)-dt);
  if(this.state.dead){
   this.deathRetry=Math.max(0,(this.deathRetry||0)-dt);
   if(this.deathPending&&this.deathRetry===0){try{this.diePhysical();}catch(error){
    this.deathRetry=.5;if(!this.deathWarning){this.deathWarning=true;console.log(`Death articulation unavailable (${this.entity.id}): ${error.message}`);}
   }}
   this.corpseAge+=dt;if(this.corpseAge>=18)this.entity.destroy();return;
  }
  if(this.state.tripped){this.tripTime-=dt;this.retry-=dt;if(this.tripTime<=0&&this.retry<=0){this.retry=.25;this.recover();}return;}
  if(this.state.dead||!this.player.valid||game.phase!=='wave')return;
  this.next-=dt;this.attack-=dt;this.stun=Math.max(0,this.stun-dt);
  const m=this.entity.character,t=this.entity.transform,p=this.player.transform.position,u=m.up;
  if((this.forceTrip||this.shouldTrip(dt))&&this.trip()){this.forceTrip=false;return;}
  this.forceTrip=false;
  if(this.pushTime>0){this.pushTime=Math.max(0,this.pushTime-dt);m.velocity=add(this.knock,mul(u,dot(m.velocity,u)));this.knock=mul(this.knock,Math.max(0,1-2.4*dt));this.animate('Stagger',1,dt,false);return;}
  const actualSpeed=length(tangent(m.actualDisplacement,u))/Math.max(.001,dt);
  if(!this.soldier){
   if(this.stun>0){m.velocity=mul(u,dot(m.velocity,u));this.animate('Stagger',1,dt,false);this.state.attackPhase='stunned';return;}
   const intent=this.brain.update(dt,u,actualSpeed);
   // Aggressive intent is still resolved by the same generic motor. Walls,
   // standing props and ledges do not disappear merely because it is an enemy.
   m.velocity=add(intent.motion,mul(u,dot(m.velocity,u)));this.turn(intent.facing,dt);this.strike(intent.strike);
   this.state.awareness=this.brain.visible?'pursue':this.brain.memory>0?'hunt':'search';this.state.attackPhase=intent.attackPhase;
   if(actualSpeed>.32)this.locomoting=true;else if(actualSpeed<.13)this.locomoting=false;
   const attacking=intent.clip==='Attack';
   const clip=attacking?'Attack':this.locomoting?this.brain.variant==='brute'?'Shamble':'ZombieRun':'ZombieIdle';
   const punchRate=this.brain.variant==='brute'?.70:this.brain.variant==='stalker'?.90:1.05;
   this.animate(clip,attacking?punchRate:clip==='ZombieIdle'?1:actualSpeed/(clip==='Shamble'?1.6:3.7),dt,!attacking);return;
  }
  const offset=sub(p,t.position),distance=length(offset),toward=norm(tangent(offset,u));
  if(this.next<=0){this.next=.6;this.entity.navigation.setDestination(p);}
  const origin=add(t.position,mul(u,1.35)),target=add(p,mul(u,1.2));
  let visible=false;if(this.soldier||distance<1.6){const ray=physics.raycast(origin,norm(sub(target,origin)),Math.max(.1,length(sub(target,origin))+.05),{ignored:[this.entity]});visible=ray?.entity?.id===this.player.id;}
  const stop=this.stun>0||this.soldier&&visible&&distance<16||!this.soldier&&distance<1.25;
  const steering=this.entity.navigation.state.steering,desired=stop?{x:0,y:0,z:0}:tangent(steering,u);
  m.velocity=add(desired,mul(u,dot(m.velocity,u)));
  this.turn(toward,dt);if(actualSpeed>.32)this.locomoting=true;else if(actualSpeed<.13)this.locomoting=false;
  this.animate(this.locomoting?'Run':'Idle',this.locomoting?actualSpeed/3.5:1,dt);
  if(this.stun===0&&visible&&this.attack<=0&&distance<(this.soldier?20:1.6)){
   this.attack=this.soldier?1.55:.9;signals.send(this.player,'player.hurt',{damage:this.soldier?7:12});
   if(this.soldier){this.entity.playAudio();const fx=world.entity('22');if(fx.valid){fx.transform={position:target};fx.burst(5);}}
  }
 }
 presentationUpdate(dt,alpha=1){if(!this.visual?.valid||this.state.tripped||this.state.dead)return;const t=this.entity.presentedTransform;
  const position=this.returnTime>0?add(t.position,mul(this.returnOffset,this.returnTime/.45)):t.position;
  const yaw=this.previousYaw+angleDifference(this.previousYaw,this.yaw)*Math.max(0,Math.min(1,alpha));
  this.visual.transform={position,rotation:axis({x:0,y:1,z:0},yaw)};}
 destroy(){retireBones(this.entity);if(this.token)signals.unsubscribe(this.token);}
}
