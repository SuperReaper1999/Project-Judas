import {world,input} from 'judas';
import {add,sub,length,rotate,qm,axis} from './math.js';
export default class Rover {
 constructor(p){this.p=p;this.entity=world.entity('25000');this.active=false;}
 tick(){const e=this.entity;if(!e?.valid)return false;const near=length(sub(e.transform.position,this.p.entity.transform.position))<3.5;
  if(input.pressed('drive_use')&&(near||this.active)){if(this.active){this.active=false;this.p.entity.transform={position:add(e.transform.position,rotate(e.transform.rotation,{x:2,y:.4,z:0})),rotation:e.transform.rotation};this.p.entity.character.enabled=true;this.p.entity.character.velocity=e.velocity;}else{this.p.grab.drop();this.active=true;this.p.entity.character.enabled=false;}}
  const throttle=this.active?input.axis('move_y'):0,brake=!this.active||input.held('jump')?1:0;
  e.powertrain.set({sources:[{name:'engine',input:Math.max(.08,Math.abs(throttle)),starter:1}],links:[{name:'gearbox',ratio:(throttle<0?-1:1)/(this.p.campaign.state.built.includes('rover')?3.4:4.5)},{name:'clutch',engagement:this.active?1:0}],brakes:[{name:'left',engagement:brake},{name:'right',engagement:brake}]});
  for(let i=0;i<4;i++)world.entity(String(25001+i)).wheel.set({steeringAngle:i<2&&this.active?-.42*input.axis('move_x'):0,brakeTorque:brake*750});
  if(this.active){this.p.entity.transform={position:add(e.transform.position,rotate(e.transform.rotation,{x:0,y:.5,z:0})),rotation:e.transform.rotation};this.p.campaign.dialogue='ROVER / WASD drive / Space brake / K leave';}return this.active;
 }
 pose(){const t=this.entity.presentedTransform,q=qm(t.rotation,axis({x:0,y:1,z:0},this.p.state.yaw));const eye=add(t.position,rotate(q,{x:0,y:1.5,z:0}));return {position:add(t.position,rotate(q,{x:0,y:3.5,z:7})),eye,direction:rotate(q,{x:0,y:0,z:-1}),rotation:q};}
}
