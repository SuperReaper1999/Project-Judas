import {world,physics} from 'judas';
import {add,mul,length,norm,qm,axis,rotate} from './math.js';
export default class Camera {
 constructor(player){this.player=player;this.third=false;}
 pose(presented=false){const t=presented?this.player.entity.presentedTransform:this.player.entity.transform,s=this.player.state;
  const rotation=qm(qm(t.rotation,axis({x:0,y:1,z:0},s.yaw)),axis({x:1,y:0,z:0},s.pitch));
  const eye=add(t.position,rotate(t.rotation,{x:0,y:1.65,z:0})),direction=rotate(rotation,{x:0,y:0,z:-1});
  let position=eye;
  if(this.third){const boom=rotate(rotation,{x:.8,y:.45,z:3.4}),distance=length(boom),backwards=norm(boom);
   const obstruction=physics.sphereCast(eye,.18,backwards,distance,{ignored:[this.player.entity]});
   position=add(eye,mul(backwards,obstruction?Math.max(0,obstruction.distance-.1):distance));}
  return {position,rotation,direction,eye};
 }
 publish(presented=false){const p=this.pose(presented);world.setView(p,74-(this.player.weapon?.aimBlend||0)*12);return p;}
}
