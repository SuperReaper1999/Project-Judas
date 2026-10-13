import {input,world,physics,ui,saves} from 'judas';
import {add,mul,sub,dot,length,norm,bounded,qm,rotate,axis,matrix,attitude} from './math.js';
export const properties={speed:{type:'number',default:5},launch:{type:'number',default:4}};
export default class {
 constructor({entity,properties}){this.entity=entity;this.props=properties;this.state={yaw:0,pitch:.1,held:null};this.held=null;}
 start(){input.pointerCapture=true;ui.debugOverlayVisible=false;}
 restore(){this.start();this.held=this.state.held?saves.resolve(this.state.held):null;}
 ray(presented=false){const t=presented?this.entity.presentedTransform:this.entity.transform;const rotation=qm(qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));return {origin:add(t.position,rotate(t.rotation,{x:0,y:.7,z:0})),direction:rotate(rotation,{x:0,y:0,z:-1}),rotation};}
 update(dt){if(ui.get('lab')?.modal)return;this.state.yaw-=input.axis('look_x')*.0021+input.axis('look_stick_x')*2*dt;this.state.pitch=Math.max(-1.5,Math.min(1.5,this.state.pitch-input.axis('look_y')*.0021-input.axis('look_stick_y')*2*dt));if(this.held&&!this.held.valid)this.held=null;
 if(input.pressed('interact')){if(this.held)this.held=null;else{const r=this.ray(),hit=physics.raycast(r.origin,r.direction,5,{ignored:[this.entity]});if(hit?.entity?.hasTag('pickup'))this.held=hit.entity;}}
 if(input.pressed('throw')&&this.held){this.held.applyImpulse(mul(this.ray().direction,this.held.mass*5));this.held=null;}this.state.held=this.held?saves.reference(this.held):null;
 }
 fixedUpdate(dt){const motor=this.entity.character;if(!motor)return;const s=motor.state,up=s.up,t=this.entity.transform,heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));const tangent=v=>sub(v,mul(up,dot(v,up)));let direction=add(mul(norm(tangent(rotate(heading,{x:0,y:0,z:-1}))),input.axis('move_y')),mul(norm(tangent(rotate(heading,{x:1,y:0,z:0}))),input.axis('move_x')));if(length(direction)>1)direction=norm(direction);let velocity=motor.velocity;if(s.supported){const own=tangent(sub(velocity,s.supportVelocity)),change=sub(mul(direction,this.props.speed),own);velocity=add(s.supportVelocity,add(own,mul(norm(change),Math.min(length(change),24*dt))));if(input.pressed('jump'))velocity=add(velocity,mul(up,this.props.launch));}else if(length(direction)>0)velocity=add(velocity,mul(direction,5*dt));motor.velocity=velocity;
 if(this.held?.valid){const r=this.ray(),target=add(r.origin,mul(r.direction,1.8));this.held.applyForce(mul(bounded(add(mul(sub(target,this.held.transform.position),70),mul(sub(motor.velocity,this.held.velocity),17)),70),this.held.mass));const alpha=bounded(sub(mul(attitude(r.rotation,this.held.transform.rotation),64),mul(this.held.angularVelocity,16)),90);this.held.applyTorque(matrix(this.held.inertiaWorld,alpha));}
 }
 presentationUpdate(){const r=this.ray(true);world.setView({position:r.origin,rotation:r.rotation},70);}
 destroy(){input.pointerCapture=false;world.clearView();}
}
