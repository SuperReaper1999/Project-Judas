// Attach to an entity with CharacterMotor + an extracted Walk clip, such as
// the Motion Studio performer. Game behaviour remains in this project script.
/** @param {import('judas').Quat} q @param {import('judas').Vec3} v */
function rotate(q,v){
  const tx=2*(q.y*v.z-q.z*v.y),ty=2*(q.z*v.x-q.x*v.z),tz=2*(q.x*v.y-q.y*v.x);
  return {x:v.x+q.w*tx+q.y*tz-q.z*ty,y:v.y+q.w*ty+q.z*tx-q.x*tz,z:v.z+q.w*tz+q.x*ty-q.y*tx};
}
export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}){this.entity=entity;this.state={controlled:false,markers:0,moved:false,frameGuard:false};this.startZ=entity.transform.position.z;}
  update(){
    const a=this.entity.animation;if(!a?.info.ready||this.state.frameGuard)return;
    try{a.advance(1/60);}catch{this.state.frameGuard=true;}
  }
  /** @param {number} dt */
  fixedUpdate(dt){
    const a=this.entity.animation,m=this.entity.character;if(!a?.info.ready||!m)return;
    if(!this.state.controlled){a.crossFade('Walk',.15);a.loop=true;this.state.controlled=true;}
    const sample=a.advance(dt);if(sample.overflow)throw Error('marker interval overflow');
    m.displace(rotate(this.entity.transform.rotation,sample.motion.translation));
    for(const event of sample.events)if(event.source==='base'&&event.weight>.5&&!event.reverse)this.state.markers++;
    this.state.moved=Math.abs(this.entity.transform.position.z-this.startZ)>.1;
  }
}
