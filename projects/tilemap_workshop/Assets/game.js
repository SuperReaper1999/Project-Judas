// Reusable project policy. Native MK2 owns collision, gravity and physical state.
import {input,physics,world} from 'judas';
export const properties={sideView:{type:'boolean',default:true},speed:{type:'number',default:4},
  acceleration:{type:'number',default:35},jumpSpeed:{type:'number',default:6}};
/** @typedef {import('judas').Vec3} Vec3 */
/** @typedef {import('judas').Quat} Quat */
/** @param {Vec3} a @param {Vec3} b */
const add=(a,b)=>({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
/** @param {Vec3} a @param {number} k */
const mul=(a,k)=>({x:a.x*k,y:a.y*k,z:a.z*k});
/** @param {Vec3} a @param {Vec3} b */
const dot=(a,b)=>a.x*b.x+a.y*b.y+a.z*b.z;
/** @param {Vec3} a @param {Vec3} b */
const cross=(a,b)=>({x:a.y*b.z-a.z*b.y,y:a.z*b.x-a.x*b.z,z:a.x*b.y-a.y*b.x});
/** @param {Quat} q @param {Vec3} v */
const rotate=(q,v)=>{const t=mul(cross(q,v),2);return add(v,add(mul(t,q.w),cross(q,t)));};
/** @param {Vec3} v */
const unit=v=>mul(v,1/Math.max(.000001,Math.sqrt(dot(v,v))));
export default class {
  /** @param {import('judas').ScriptContext<{sideView:boolean;speed:number;acceleration:number;jumpSpeed:number}>} context */
  constructor({entity,properties}){this.entity=entity;this.properties=properties;this.state={grounded:false,landings:0};}
  /** @param {number} dt */
  fixedUpdate(dt){
    const body=this.entity.body2D;
    if(!body||body.kind!=='capsule'||!body.lockRotation)throw new Error('2D player requires a rotation-locked native capsule collider');
    const pose=this.entity.transform,right=rotate(pose.rotation,{x:1,y:0,z:0}),up=rotate(pose.rotation,{x:0,y:1,z:0});
    const velocity=this.entity.velocity,x=input.axis('move_x'),y=input.axis('move_y');
    let desired,change;
    if(this.properties.sideView){
      const halfHeight=body.vertices[1].y;
      const hit=physics.cast2D({kind:'capsule',radius:body.radius,halfHeight},pose,mul(up,-1),.12,{ignored:[this.entity]});
      const grounded=!!hit&&dot(hit.normal,up)>.55&&dot(velocity,up)<1;
      if(grounded&&!this.state.grounded)this.state.landings++;
      this.state.grounded=grounded;
      const ground=grounded&&hit.entity?hit.entity.pointVelocity(hit.point):{x:0,y:0,z:0};
      const tangent=grounded?unit(add(right,mul(hit.normal,-dot(right,hit.normal)))):right;
      const relative=add(velocity,mul(ground,-1));
      change=mul(tangent,x*this.properties.speed-dot(relative,tangent));
      if(grounded&&input.pressed('jump')){
        // An impulse changes momentum; no pose correction or velocity reset.
        this.entity.applyImpulse(mul(up,this.entity.mass*(this.properties.jumpSpeed-dot(relative,up))));
        this.state.grounded=false;
      }
    }else{
      const length=Math.max(1,Math.hypot(x,y));desired=add(mul(right,x/length*this.properties.speed),mul(up,y/length*this.properties.speed));
      change=add(desired,mul(velocity,-1));
    }
    const length=Math.sqrt(dot(change,change)),limit=this.properties.acceleration*dt;
    if(length>limit)change=mul(change,limit/length);
    this.entity.applyForce(mul(change,this.entity.mass/dt));
    if(x)this.entity.setSprite({flipX:x<0});
  }
  presentationUpdate(){
    const pose=this.entity.presentedTransform,forward=rotate(pose.rotation,{x:0,y:0,z:1});
    world.setView({position:add(pose.position,mul(forward,15)),rotation:pose.rotation},60,
      {projection:'orthographic',height:10,near:.01,far:100});
  }
  destroy(){world.clearView();}
}
