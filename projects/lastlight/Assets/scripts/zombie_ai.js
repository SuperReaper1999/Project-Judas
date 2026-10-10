import {world,physics,navigation} from 'judas';
import {game} from './game.js';
import {add,sub,mul,dot,length,norm,tangent} from './math.js';

// Ordinary game behaviour. Navigation suggests a route; the motor still owns
// collision-aware displacement, including a blocked lunge or a heavy shove.
export const zombieTypes={
 runner:{health:70,speed:3.65,damage:11,windup:.28,lunge:5.8,lungeTime:.24,reach:1.55,cooldown:1.35,shoveScale:1},
 brute:{health:140,speed:2.15,damage:22,windup:.52,lunge:3.3,lungeTime:.2,reach:1.75,cooldown:1.8,shoveScale:.62},
 stalker:{health:85,speed:2.8,damage:14,windup:.34,lunge:6,lungeTime:.25,reach:1.6,cooldown:1.6,shoveScale:.9}
};

export function spawnHash(id){
 let hash=2166136261;
 for(const c of String(id))hash=Math.imul(hash^c.charCodeAt(0),16777619);
 return hash>>>0;
}
const zero=()=>({x:0,y:0,z:0});
const cross=(a,b)=>({x:a.y*b.z-a.z*b.y,y:a.z*b.x-a.x*b.z,z:a.x*b.y-a.y*b.x});

export default class ZombieAI {
 constructor(entity,player,variant='auto'){
  this.entity=entity;this.player=player;this.seed=spawnHash(entity.id);
  this.variant=zombieTypes[variant]?variant:['runner','brute','stalker'][this.seed%3];
  this.type=zombieTypes[this.variant];this.side=this.seed&1?1:-1;
  this.sense=(this.seed%19)*.011;this.repath=(this.seed%11)*.018;
  this.shots=game.shots;this.visible=false;this.memory=8;
  // The wave's alarm gives newly arriving creatures one initial search point,
  // not permanent knowledge of the player's position through every wall.
  this.lastKnown=player.transform.position;this.goal=this.lastKnown;
  this.cooldown=.65+(this.seed%7)*.1;this.searchClock=0;this.searchIndex=0;
  this.flankClock=0;this.flankTime=0;this.flankGoal=null;
  this.obstructionClock=.2;this.blockedTime=0;this.attack=null;
 }

 clearAttack(){this.attack=null;this.cooldown=Math.max(this.cooldown,.65);}
 alert(position,urgent=false){this.lastKnown={...position};this.memory=7;if(urgent)this.repath=0;}
 lineOfSight(position,up){
  const from=add(this.entity.transform.position,mul(up,1.35));
  const to=add(position,mul(up,1.2)),offset=sub(to,from),distance=length(offset);
  if(distance<.02)return true;
  const hit=physics.raycast(from,norm(offset),distance+.04,{ignored:[this.entity]});
  return hit?.entity?.id===this.player.id;
 }

 sensePlayer(dt,position,up,distance){
  this.memory=Math.max(0,this.memory-dt);this.sense-=dt;
  if(this.sense>0)return;
  this.sense=.2+(this.seed%7)*.013;
  this.visible=distance<34&&this.lineOfSight(position,up);
  const fired=game.shots!==this.shots;this.shots=game.shots;
  const moving=length(tangent(this.player.character.velocity,up))>1.2;
  if(this.visible)this.alert(position);
  else if(game.noiseTime>0&&game.noisePosition&&length(sub(game.noisePosition,this.entity.transform.position))<43)
   this.alert(game.noisePosition);
  else if(game.footstepTime>0&&game.footstepPosition&&length(sub(game.footstepPosition,this.entity.transform.position))<9)
   this.alert(game.footstepPosition);
  else if(fired&&distance<43||moving&&distance<8.5)this.alert(position);
 }

 chooseGoal(dt,position,up,distance){
  this.flankClock-=dt;this.flankTime=Math.max(0,this.flankTime-dt);
  if(this.variant==='stalker'&&this.visible&&distance>3&&distance<15&&this.flankClock<=0){
   const viewForward=norm(tangent(world.viewRay.direction,up));
   const right=norm(cross(viewForward,up));
   const candidate=add(add(position,mul(right,this.side*3.4)),mul(viewForward,-2.6));
   const sample=navigation.sample(candidate,1.5);
   this.flankGoal=sample?sample.position:null;this.flankTime=sample?1.7:0;this.flankClock=4.2;
  }
  if(this.flankGoal&&this.flankTime>0&&distance>2.6){this.goal=this.flankGoal;return;}
  if(this.visible||this.memory>0){this.goal=this.lastKnown;return;}
  this.searchClock-=dt;
  const arrived=length(tangent(sub(this.lastKnown,this.entity.transform.position),up))<1.5;
  if(arrived&&this.searchClock<=0){
   this.searchClock=2.1;const angle=(this.searchIndex++%4)*Math.PI/2+(this.seed%6)*.35;
   // This town has authored uniform gravity; sampled navigation positions,
   // not manually offset transforms, determine traversable investigation points.
   const candidate=add(this.lastKnown,{x:Math.cos(angle)*2.2,y:0,z:Math.sin(angle)*2.2});
   const sample=navigation.sample(candidate,1.5);this.goal=sample?sample.position:this.lastKnown;
  }
 }

 beginAttack(direction,obstacle=null){
  this.attack={time:0,hit:false,direction:norm(direction),obstacle,
   windup:obstacle ? .48 : this.type.windup,lungeTime:obstacle ? 0 : this.type.lungeTime};
 }

 attackStep(dt,position,up){
  const a=this.attack;a.time+=dt;
  const impact=a.windup+(a.obstacle ? .1 : a.lungeTime*.6);
  let strike=null;
  if(!a.hit&&a.time>=impact){
   a.hit=true;
   if(a.obstacle){
    const body=a.obstacle.entity;
    if(body?.valid&&length(sub(body.transform.position,this.entity.transform.position))<2.5){
     const from=add(this.entity.transform.position,mul(up,.8)),toward=sub(body.transform.position,from),distance=length(toward);
     const contact=physics.sphereCast(from,.2,distance>.01?norm(toward):a.direction,Math.min(1.35,distance+.2),{ignored:[this.entity]});
     if(contact?.entity?.id===body.id)strike={obstacle:body,point:contact.point,direction:a.direction};
    }
   }else {
    const separation=sub(position,this.entity.transform.position),offset=tangent(separation,up);
    // A planar distance alone would let a creature punch someone several metres
    // above it on an open ledge. These are game-authored arm/reach limits, not
    // a new collision rule; fresh visibility must still clear real geometry.
    if(Math.abs(dot(separation,up))<=.95&&length(separation)<=this.type.reach&&
       (length(offset)<.2||dot(norm(offset),a.direction)>.25)&&this.lineOfSight(position,up))
     strike={damage:this.type.damage};
   }
  }
  const lunging=!a.obstacle&&a.time>=a.windup&&a.time<a.windup+a.lungeTime;
  const motion=lunging?mul(a.direction,this.type.lunge):zero();
  const phase=a.time<a.windup?'windup':lunging?'lunge':'recover';
  if(a.time>a.windup+a.lungeTime+.32){this.attack=null;this.cooldown=this.type.cooldown;}
  return {motion,facing:a.direction,clip:'Attack',attackPhase:phase,strike};
 }

 obstruction(dt,motion,up,actualSpeed,aboutToLunge=false){
  this.obstructionClock-=dt;
  this.blockedTime=length(motion)>1&&actualSpeed<.4?this.blockedTime+dt:Math.max(0,this.blockedTime-2*dt);
  if(this.cooldown>0||!aboutToLunge&&(this.blockedTime<.28||this.obstructionClock>0))return null;
  if(length(motion)<.05)return null;
  this.obstructionClock=.3;
  const direction=norm(motion),origin=add(this.entity.transform.position,mul(up,.8));
  const hit=physics.sphereCast(origin,.2,direction,.85,{ignored:[this.entity]});
  const body=hit?.entity;
  if(!body?.valid||!body.hasTag('destructible')&&!(body.hasTag('physical')&&body.motionType==='dynamic'))return null;
  return {entity:body,point:hit.point,direction};
 }

 update(dt,up,actualSpeed){
  const position=this.player.transform.position;
  const separation=sub(position,this.entity.transform.position),delta=tangent(separation,up),distance=length(delta);
  this.cooldown=Math.max(0,this.cooldown-dt);this.repath-=dt;
  this.sensePlayer(dt,position,up,distance);
  if(this.attack)return this.attackStep(dt,position,up);
  this.chooseGoal(dt,position,up,distance);
  if(this.repath<=0){this.repath=.48+(this.seed%5)*.045;this.entity.navigation.setDestination(this.goal);}
  const steering=tangent(this.entity.navigation.state.steering,up);
  const motion=length(steering)>.08?mul(norm(steering),Math.min(length(steering),this.type.speed)):zero();
  // A short locked-direction lunge has a visible stationary anticipation. It
  // is motor velocity, so an obstruction blocks it rather than being teleported.
  const lungeRange=this.variant==='brute'?2.25:2.8;
  const aboutToLunge=this.visible&&distance<lungeRange&&Math.abs(dot(separation,up))<=.95&&this.cooldown<=0;
  // A low barricade can leave chest-height LOS clear while blocking the motor.
  // Probe that actual near-field obstruction before committing to a lunge;
  // otherwise every attack cycle can win priority over the barricade forever.
  const barrier=this.obstruction(dt,aboutToLunge?mul(norm(delta),this.type.speed):motion,up,actualSpeed,aboutToLunge);
  if(barrier){this.beginAttack(barrier.direction,barrier);return this.attackStep(dt,position,up);}
  if(aboutToLunge){
   this.beginAttack(delta);return this.attackStep(dt,position,up);
  }
  const facing=length(motion)>.15?norm(motion):this.visible?norm(delta):null;
  return {motion,facing,clip:length(motion)>.15?(this.variant==='brute'?'Shamble':'ZombieRun'):'ZombieIdle',
   attackPhase:'',strike:null};
 }
}
