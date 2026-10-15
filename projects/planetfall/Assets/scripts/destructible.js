import {world,signals} from 'judas';
import {sub,mul,norm} from './math.js';

// Game-authored breakup: Judas supplies ordinary bodies, impulses and prefabs.
// Intact geometry disappears only after its separately simulated pieces exist.
export const properties={
 health:{type:'number',default:60},
 debris:{type:'string',default:''},
 cleanup:{type:'boolean',default:false},
 lifetime:{type:'number',default:12}
};
const groups=[];
const MAX_GROUPS=12;
function registerGroup(entity){
 for(let i=groups.length-1;i>=0;i--)if(!groups[i].valid)groups.splice(i,1);
 groups.push(entity);
 // A bounded game budget, not a special engine physics rule.
 while(groups.length>MAX_GROUPS){const old=groups.shift();if(old.valid)old.destroy();}
}
export default class Destructible {
 constructor({entity,properties}){
  this.entity=entity;this.props=properties;
  this.state={health:properties.health,pending:false,age:0};
 }
 register(){
  if(this.props.cleanup)registerGroup(this.entity);
  else this.token=signals.subscribe('range.hit');
 }
 start(){this.register();}
 restore(){this.register();}
 onSignal(event){
  if(event.name!=='range.hit'||this.state.pending||this.props.cleanup)return;
  const damage=event.payload?.damage;
  if(typeof damage!=='number'||!Number.isFinite(damage)||damage<=0)return;
  this.state.health-=damage;
  if(this.state.health<=0)this.state.pending=true;
 }
 fixedUpdate(dt){
  if(this.props.cleanup){
   this.state.age+=dt;
   if(this.state.age>=this.props.lifetime)this.entity.destroy();
   return;
  }
  if(!this.state.pending)return;
  // Capture actual motion after the damaging impulse has gone through physics.
  const pose=this.entity.transform;
  // Static destructible doors have no angular-velocity getter. Their actual
  // point velocity is still provided by Judas; rotating/moving bodies retain
  // their real world angular motion instead of sharing a static-only shortcut.
  const angular=this.entity.motionType==='static'?{x:0,y:0,z:0}:this.entity.angularVelocity;
  const pieces=world.spawnPrefab(this.props.debris,{position:pose.position,rotation:pose.rotation});
  for(const piece of pieces.children){
   if(!piece.valid||piece.motionType!=='dynamic')continue;
   const point=piece.transform.position;
   piece.velocity=this.entity.pointVelocity(point);
   piece.angularVelocity=angular;
   // Small authored separation impulse exposes the panels instead of leaving
   // adjacent collision boxes touching. The impulse is integrated normally.
   const out=norm(sub(point,pose.position));
   piece.applyImpulse(mul(out,piece.mass*.35));
  }
  const feedback=world.entity('20');
  if(feedback?.valid){feedback.transform={position:pose.position};feedback.burst(20);feedback.playAudio();}
  this.entity.destroy();
 }
 destroy(){if(this.token)signals.unsubscribe(this.token);}
}
