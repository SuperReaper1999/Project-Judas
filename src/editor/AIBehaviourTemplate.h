#pragma once
// Copyable project gameplay template, instantiated with registered definition metadata.
constexpr const char* kAIPatrolHideTemplate=R"JSAI(
// Generated two-action starter. This is ordinary project gameplay, not native AI policy.
import {UtilityAgent, AITargetReservations, entity, physics, signals} from 'judas';
import definition from '__DEFINITION_IMPORT__';
export const aiDefinition = '__DEFINITION_ID__';
export const properties = {
  patrolPoint:{type:'entity',default:null},
  returnPoint:{type:'entity',default:null},
  coverPoint:{type:'entity',default:null},
  speed:{type:'number',default:2},
  recoveryPerSecond:{type:'number',default:.15},
  planar:{type:'boolean',default:false}
};
const zero={x:0,y:0,z:0};
const add=(a,b)=>({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
const scale=(a,n)=>({x:a.x*n,y:a.y*n,z:a.z*n});
const subtract=(a,b)=>add(a,scale(b,-1));
const dot=(a,b)=>a.x*b.x+a.y*b.y+a.z*b.z;
const length=a=>Math.sqrt(dot(a,a));
const clamp=x=>Math.max(0,Math.min(1,x));

export default class PatrolAndHide {
  constructor({entity:owner,properties}) {
    this.entity=owner;this.properties=properties;
    this.state={health:1,brain:null,arrived:false};
    this.intent=zero;
  }
  createBrain() {
    this.leases=new AITargetReservations();
    const enter=(context,choice)=>{this.resource='target:'+choice.target;};
    const stop=()=>{this.intent=zero;if(this.resource)this.leases.release(this.resource);this.resource='';};
    this.brain=new UtilityAgent(definition,{
      patrol:{start:enter,update:(dt,context,choice)=>{this.walk(choice.target,dt);},stop},
      hide:{start:enter,update:(dt,context,choice)=>{
        if(this.walk(choice.target,dt)) {
          this.state.health=clamp(this.state.health+dt*Math.max(0,this.properties.recoveryPerSecond));
          if(this.state.health>=.9)this.brain.finish('success',context);
        }
      },stop}
    },'NPC '+this.entity.id+' - patrol / hide');
    this.damageToken=signals.subscribe('ai.damage');
  }
  start(){this.createBrain();}
  restore(){this.createBrain();if(this.state.brain)this.brain.restore(this.state.brain);}
  onSignal(event) {
    // Use signals.send(npc,'ai.damage',{amount:.6}) from your combat script.
    const amount=event.payload?.amount;
    if(event.name==='ai.damage'&&Number.isFinite(amount)&&amount>0)
      this.state.health=clamp(this.state.health-amount);
  }
  candidates() {
    const candidate=(action,target,facts,eligible,reason)=>{
      const exists=!!target?.valid, available=exists&&this.leases.available('target:'+target.id);
      return {action,target:target?.id??'',facts,eligible:eligible&&exists&&available,
        reason:!exists?'Assign a live '+(action==='patrol'?'patrol':'cover')+' target in Scripts properties':
          !available?'Target is reserved by another actor':reason};
    };
    const away=target=>target?.valid&&length(this.deltaTo(target))>.25;
    return [
      candidate('patrol',this.properties.patrolPoint,{},away(this.properties.patrolPoint),'Patrol point already reached'),
      candidate('patrol',this.properties.returnPoint,{},away(this.properties.returnPoint),'Return point already reached'),
      candidate('hide',this.properties.coverPoint,{health:this.state.health},this.state.health<.9,'Healthy enough to patrol')
    ];
  }
  deltaTo(target) {
    const pose=this.entity.transform;
    let delta=subtract(target.transform.position,pose.position);
    const motor=this.entity.character;
    if(motor)delta=subtract(delta,scale(motor.up,dot(delta,motor.up)));
    if(this.properties.planar) {
      // Planar bodies keep their authored plane. cast2D uses that pose/frame.
      const normal=this.entity.body2D.normal;
      delta=subtract(delta,scale(normal,dot(delta,normal)));
    }
    return delta;
  }
  walk(targetId,dt) {
    if(!this.leases.acquire('target:'+targetId,1)){this.intent=zero;this.brain.finish('failure',this);return false;}
    const target=entity(targetId);
    if(!target?.valid){this.intent=zero;this.brain.finish('failure',this);return false;}
    const pose=this.entity.transform;
    const delta=this.deltaTo(target);
    const distance=length(delta);
    this.state.arrived=distance<.15;
    if(this.state.arrived){this.intent=zero;return true;}
    const speed=Math.max(0,this.properties.speed);
    this.intent=scale(delta,Math.min(speed,Math.max(0,distance-.1)/dt)/distance);
    if(this.properties.planar) {
      const direction=scale(delta,1/distance);
      const body=this.entity.body2D;
      const shape=body.kind==='circle'?{kind:'circle',radius:body.radius}:
        body.kind==='capsule'?{kind:'capsule',radius:body.radius,halfHeight:Math.abs(body.vertices[1].y)}:
        {kind:'convex',vertices:body.vertices};
      const hit=physics.cast2D(shape,pose,direction,speed*dt,{ignored:[this.entity],includeSensors:false});
      if(hit&&hit.distance<speed*dt)this.intent=zero;
    }
    return false;
  }
  fixedUpdate(dt) {
    this.intent=zero;
    this.brain.tick(dt,()=>this.candidates(),this);
    const motor=this.entity.character;
    if(motor) {
      // Preserve the gravity-axis velocity; the motor owns sweep/slide and gravity.
      motor.velocity=add(this.intent,scale(motor.up,dot(motor.velocity,motor.up)));
    } else if(this.properties.planar)this.entity.setKinematicVelocity(this.intent,zero);
    this.state.brain=this.brain.snapshot();
  }
  destroy(){this.brain?.dispose();this.leases?.dispose();if(this.damageToken)signals.unsubscribe(this.damageToken);}
}
)JSAI";
