export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}) {
    this.entity=entity;
    this.state={configured:false,impactCount:0,impactWeight:0,lastReceiver:''};
  }
  start() {
    // Add a Particle Emitter component first. This changes an existing component;
    // it never creates a rigid body per particle or enables Advanced Fluids.
    this.state.configured=this.entity.setParticles({
      backend:'cpu',enabled:true,rate:8,maxParticles:128,lifetime:3,
      collision:true,collisionRadius:0.025,response:'bounce',
      restitution:0.3,friction:0.2,collisionLayers:['Default'],
      impacts:'reliable',impactCellSize:0.25,impactWeight:1,maxImpactCells:128
    });
    if(this.state.configured)this.entity.burst(12);
  }
  fixedUpdate() {
    // Draining lets a full reliable queue resume its retained simulation step.
    // Game code can resolve impact.receiver and apply its own receiver rules.
    for(const impact of this.entity.takeParticleImpacts()) {
      this.state.impactCount+=impact.count;
      this.state.impactWeight+=impact.weight;
      this.state.lastReceiver=impact.receiver;
    }
  }
}
