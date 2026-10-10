// Small project-authored climb. Native motor owns every collision-resolved step.
import {physics} from 'judas';
import {add,mul,sub,dot,length,norm,tangent} from './math.js';
export default class Climb {
 constructor(player){this.player=player;this.active=null;this.cooldown=0;}
 begin(forward){if(this.active||this.cooldown>0)return false;
  const p=this.player.entity,t=p.transform,u=p.character.up,filter={ignored:[p]};
  const facing=norm(tangent(forward,u));
  const wall=physics.raycast(add(t.position,mul(u,.6)),facing,1.05,filter);
  if(!wall||dot(wall.normal,u)>.35)return false;
  const above=add(add(t.position,mul(facing,wall.distance+.55)),mul(u,2));
  const top=physics.raycast(above,mul(u,-1),2,filter);
  if(!top||dot(top.normal,u)<.75)return false;
  const rise=dot(sub(top.point,t.position),u);if(rise<.35||rise>1.55)return false;
  const target=add(top.point,mul(u,.04));
  const occupied=physics.capsuleCast({position:add(target,mul(u,.91)),rotation:t.rotation},.28,.6,u,0,filter);
  if(occupied)return false;
  this.active={start:t.position,target,up:u,phase:0,time:0,last:t.position,stall:0};
  p.character.configure({gravityScale:0});this.cooldown=.5;return true;
 }
 step(dt){this.cooldown=Math.max(0,this.cooldown-dt);if(!this.active)return false;
  const a=this.active,p=this.player.entity,m=p.character,pos=p.transform.position;a.time+=dt;
  // Lift first, then cross the lip: never drag a capsule through the wall.
  const lift=add(a.start,mul(a.up,dot(sub(a.target,a.start),a.up)+.06));
  const goal=a.phase===0?lift:a.target;const delta=sub(goal,pos);
  if(length(delta)<.06){if(a.phase===0){a.phase=1;a.time=0;}else{this.end();return false;}}
  else{m.velocity=mul(norm(delta),Math.min(4,length(delta)/dt));
   a.stall=length(sub(pos,a.last))<.002?a.stall+dt:0;a.last=pos;
   if(a.time>1.3||a.stall>.3){this.end();return false;}}
  return true;
 }
 end(){this.active=null;this.player.entity.character.configure({gravityScale:1});this.player.entity.character.velocity={x:0,y:0,z:0};}
}
