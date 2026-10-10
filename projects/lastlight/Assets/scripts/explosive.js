import {physics,world} from 'judas';import {detonate} from './explosives.js';import {sub,length,norm,add,mul} from './math.js';
export const properties={kind:{type:'string',default:'grenade'},fuse:{type:'number',default:2.4},radius:{type:'number',default:5.5},damage:{type:'number',default:100}};
export default class Explosive {
 constructor({entity,properties,initialState}){this.entity=entity;this.props=properties;this.state={owner:initialState?.owner||null,previous:initialState?.previous||null,age:0,spent:false};this.contact=false;}
 onCollisionEnter(event){if(this.props.kind==='rocket'&&event.other?.id!==this.state.owner)this.contact=true;}
 fixedUpdate(dt){if(this.state.spent)return;this.state.age+=dt;const current=this.entity.transform.position;let point=current;
  if(this.props.kind==='rocket'&&this.state.previous){const offset=sub(current,this.state.previous),distance=length(offset);
   const ignored=[this.entity];if(this.state.owner){const owner=world.entity(this.state.owner);if(owner.valid)ignored.push(owner);}
   if(distance>.001){const hit=physics.sphereCast(this.state.previous,.12,norm(offset),distance,{ignored});if(hit){this.contact=true;point=add(hit.point,mul(hit.normal,.04));}}
  }
  this.state.previous=current;
  if(this.contact||this.state.age>=this.props.fuse){this.state.spent=true;detonate(this.entity,this.state.owner,point,this.props.radius,this.props.damage);this.entity.destroy();}
 }
}
