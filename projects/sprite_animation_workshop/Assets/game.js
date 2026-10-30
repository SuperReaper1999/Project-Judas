import Motion from './player-motion.js';
import {input,physics} from 'judas';
export {properties} from './player-motion.js';
export default class extends Motion {
 constructor(context){super(context);this.state.strikes=0;this.state.impacts=0;this.state.footsteps=0;}
 fixedUpdate(dt){
  super.fixedUpdate(dt);
  const a=this.entity.spriteAnimation;if(!a||a.error)throw Error(a?a.error:'sprite sheet unavailable');if(!a.ready)return;
  // Each fixed callback reads the previous completed sample once. Rendering and
  // editor previews never run this code or dispatch these gameplay decisions.
  for(const event of a.events){
   if(event.name==='footstep')this.state.footsteps++;
   if(event.name==='strike'){
    this.state.strikes++;
    const pose=this.entity.transform,q=pose.rotation,sign=this.entity.sprite.flipX?-1:1;
    const forward={x:sign*(1-2*(q.y*q.y+q.z*q.z)),y:sign*2*(q.x*q.y+q.w*q.z),z:sign*2*(q.x*q.z-q.w*q.y)};
    const hit=physics.cast2D({kind:'circle',radius:.2},pose,forward,1.6,{ignored:[this.entity]});
    if(hit&&hit.entity&&hit.entity.mass>0){hit.entity.applyImpulse({x:forward.x*6,y:forward.y*6,z:forward.z*6});this.state.impacts++;}
   }
  }
  if(input.pressed('interact')&&(a.clip!=='interact'||a.finished)){
    this.entity.setSpriteAnimation({clip:'interact',time:0,playing:true,loop:false});return;
  }
  if(a.clip==='interact'&&!a.finished)return;
  const moving=Math.abs(input.axis('move_x'))+Math.abs(input.axis('move_y'))>.05;
  const clip=this.properties.sideView&&!this.state.grounded?'jump':moving?'run':'idle';
  if(a.clip!==clip||!a.playing)this.entity.setSpriteAnimation({clip,time:0,playing:true,loop:clip!=='jump'});
 }
}
