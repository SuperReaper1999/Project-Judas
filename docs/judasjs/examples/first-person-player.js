// Editor-owned template copied into the game project. Edit it freely.
import {input, world, physics, console, ui} from 'judas';
export const properties = {speed: {type: 'number', default: 4}, jumpSpeed: {type: 'number', default: 5},
  thirdPerson: {type: 'boolean', default: false}};
/** @typedef {import('judas').Vec3} Vec3 */
/** @typedef {import('judas').Quat} Quat */
/** @param {Vec3} a @param {Vec3} b */
const add = (a, b) => ({x:a.x+b.x, y:a.y+b.y, z:a.z+b.z});
/** @param {Vec3} v @param {number} s */
const mul = (v, s) => ({x:v.x*s, y:v.y*s, z:v.z*s});
/** @param {Vec3} a @param {Vec3} b */
const dot = (a, b) => a.x*b.x+a.y*b.y+a.z*b.z;
/** @param {Vec3} v */
const length = v => Math.sqrt(dot(v,v));
/** @param {Vec3} v */
const normal = v => length(v)>1e-6 ? mul(v,1/length(v)) : {x:0,y:0,z:0};
/** @param {Vec3} v @param {Vec3} up */
const tangent = (v, up) => add(v,mul(up,-dot(v,up)));
/** @param {Quat} a @param {Quat} b */
const qm = (a,b) => ({w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,
  x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y, y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
  z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w});
/** @param {Vec3} a @param {number} t */
const axis = (a,t) => ({w:Math.cos(t/2),...mul(a,Math.sin(t/2))});
/** @param {Quat} q @param {Vec3} v */
const rotate = (q,v) => {const r=qm(qm(q,{w:0,...v}),{w:q.w,x:-q.x,y:-q.y,z:-q.z});return {x:r.x,y:r.y,z:r.z};};

export default class {
  /** @param {import('judas').ScriptContext<{speed:number,jumpSpeed:number,thirdPerson:boolean}>} context */
  constructor({entity,properties}) {
    this.entity=entity;this.properties=properties;
    this.state={yaw:0,pitch:0,captured:true,thirdPerson:properties.thirdPerson};
  }
  start() {
    input.pointerCapture=this.state.captured;
    console.log('Player: WASD move, mouse look, Space jump, V camera, Escape release/capture mouse. F5 stops editor Play.');
  }
  restore() {this.start();}
  uiUpdate() {
    const pause=ui.get('pause_menu');
    if(pause)this.state.captured=!pause.modal;
    else if(input.pressed('pause'))this.state.captured=!this.state.captured;
    if(input.pressed('view_toggle'))this.state.thirdPerson=!this.state.thirdPerson;
    input.pointerCapture=this.state.captured;
  }
  /** @param {number} dt */
  update(dt) {
    if(!this.state.captured)return;
    this.state.yaw-=input.axis('look_x')*.0021+input.axis('look_stick_x')*2.2*dt;
    this.state.pitch=Math.max(-1.45,Math.min(1.45,this.state.pitch-input.axis('look_y')*.0021-input.axis('look_stick_y')*2.2*dt));
  }
  /** @param {number} dt */
  fixedUpdate(dt) {
    const motor=this.entity.character;if(!motor)return;
    const up=motor.up, s=motor.state, pose=this.entity.transform;
    const heading=qm(pose.rotation,axis({x:0,y:1,z:0},this.state.yaw));
    const forward=normal(tangent(rotate(heading,{x:0,y:0,z:-1}),up));
    const right=normal(tangent(rotate(heading,{x:1,y:0,z:0}),up));
    let direction=this.state.captured?add(mul(forward,input.axis('move_y')),mul(right,input.axis('move_x'))):{x:0,y:0,z:0};
    if(length(direction)>1)direction=normal(direction);
    let velocity=motor.velocity;
    if(s.supported) {
      velocity=add(s.supportVelocity,mul(direction,this.properties.speed));
      if(this.state.captured&&input.pressed('jump'))velocity=add(velocity,mul(up,this.properties.jumpSpeed));
    } else {
      const change=add(mul(direction,this.properties.speed),mul(tangent(velocity,up),-1));
      velocity=add(velocity,mul(normal(change),Math.min(length(change),8*dt)));
    }
    motor.velocity=velocity;
  }
  presentationUpdate() {
    const pose=this.entity.presentedTransform;
    const eye=add(pose.position,rotate(pose.rotation,{x:0,y:.65,z:0}));
    const rotation=qm(qm(pose.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));
    let position=eye,distance=0;
    if(this.state.thirdPerson){
      const behind=rotate(rotation,{x:0,y:0,z:1});
      const hit=physics.sphereCast(eye,.2,behind,4,{ignored:[this.entity]});
      distance=hit?Math.max(0,hit.distance-.05):4;
      position=add(eye,mul(behind,distance));
    }
    this.entity.renderVisible=this.state.thirdPerson&&distance>.8;
    world.setView({position,rotation},70);
  }
  destroy() {input.pointerCapture=false;this.entity.renderVisible=true;world.clearView();}
}
