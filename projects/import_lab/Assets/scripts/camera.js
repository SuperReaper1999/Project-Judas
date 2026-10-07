import {world,input} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={yaw:0,pitch:0};}
 start(){input.pointerCapture=true;}
 restore(){input.pointerCapture=true;}
 update(dt){this.state.yaw-=input.axis('look_x')*.002;this.state.pitch=Math.max(-1.2,Math.min(1.2,this.state.pitch-input.axis('look_y')*.002));const q={w:Math.cos(this.state.yaw/2)*Math.cos(this.state.pitch/2),x:Math.cos(this.state.yaw/2)*Math.sin(this.state.pitch/2),y:Math.sin(this.state.yaw/2)*Math.cos(this.state.pitch/2),z:-Math.sin(this.state.yaw/2)*Math.sin(this.state.pitch/2)};const p=this.entity.transform.position,x=input.axis('move_x')*dt*4,z=-input.axis('move_y')*dt*4;this.entity.transform={position:{x:p.x+Math.cos(this.state.yaw)*x+Math.sin(this.state.yaw)*z,y:p.y,z:p.z-Math.sin(this.state.yaw)*x+Math.cos(this.state.yaw)*z}};world.setView({position:this.entity.presentedTransform.position,rotation:q},60);}
 destroy(){input.pointerCapture=false;world.clearView();}
}
