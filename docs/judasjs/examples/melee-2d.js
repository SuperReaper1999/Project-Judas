// A bounded fan of real native shape casts, not a separate collision system.
// Call once on an animation's active marker. Deduplicate across multiple markers
// in your attack state if a clip intentionally contains several active samples.
import {physics} from 'judas';
/** @param {import('judas').Entity} owner @param {import('judas').Vec3} direction */
export function swordHits(owner,direction,{reach=1.4,radius=.12,arc=0,samples=9}={}){
 if(!owner?.valid||!owner.body2D)throw Error('Sword owner must have a live planar body');
 if(!Number.isFinite(reach)||reach<=0||reach>10||!Number.isFinite(radius)||radius<0||radius>1||
    !Number.isFinite(arc)||arc<0||arc>Math.PI||!Number.isInteger(samples)||samples<1||samples>17)throw Error('Invalid bounded sword query');
 const length=Math.hypot(direction.x,direction.y);
 if(!Number.isFinite(length)||length<1e-6||direction.z!==0)throw Error('Sword direction must be nonzero in local XY');
 const pose=owner.transform,q=pose.rotation,found=new Map(),count=arc===0?1:samples;
 for(let i=0;i<count;i++){
  const angle=Math.atan2(direction.y,direction.x)+(count===1?0:arc*(i/(count-1)-.5));
  const x=Math.cos(angle),y=Math.sin(angle);
  // Rotate the local aim into the same fixed planar frame as the owner.
  const d={x:(1-2*(q.y*q.y+q.z*q.z))*x+2*(q.x*q.y-q.w*q.z)*y,
   y:2*(q.x*q.y+q.w*q.z)*x+(1-2*(q.x*q.x+q.z*q.z))*y,
   z:2*(q.x*q.z-q.w*q.y)*x+2*(q.y*q.z+q.w*q.x)*y};
  const hit=physics.cast2D({kind:'circle',radius},pose,d,reach,{ignored:[owner]});
  if(hit?.entity?.valid)found.set(hit.entity.id,hit);
 }
 return [...found.values()];
}
