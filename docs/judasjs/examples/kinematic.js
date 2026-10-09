// Attach to an ordinary box, sphere, convex or compound body. This example
// chooses motion; Judas samples it and resolves dynamic contact on fixed time.
export default class {
  /** @param {{entity: import('judas').Entity}} context */
  constructor({entity}){
    this.entity=entity;
    this.state=/** @type {{steps:number,velocityQueued:boolean,targetQueued:boolean,pointIncludesRotation:boolean,invalidRejected:boolean,completed?:boolean,actual?:import('judas').Transform,velocity?:import('judas').Vec3,angularVelocity?:import('judas').Vec3}} */
      ({steps:0,velocityQueued:false,targetQueued:false,pointIncludesRotation:false,invalidRejected:false});
  }
  start(){this.entity.setMotionType('kinematic');}
  /** @param {number} dt */
  fixedUpdate(dt){
    const e=this.entity;this.state.steps++;
    if(this.state.steps===1){
      // Linear velocity is at the COM in simulation-space metres/second;
      // angular velocity is a simulation-space vector in radians/second.
      e.setKinematicVelocity({x:.5,y:0,z:0},{x:0,y:.5,z:0});
      this.state.velocityQueued=e.kinematicMotion?.control==='velocity';
    }
    if(this.state.steps===2){
      const at=e.transform.position;
      const point=e.pointVelocity({x:at.x+1,y:at.y,z:at.z});
      this.state.pointIncludesRotation=Math.abs(point.x-.5)<.0001&&Math.abs(point.z+.5)<.0001;
    }
    if(this.state.steps===15)e.stopKinematic();
    if(this.state.steps===20){
      // A complete authored-pivot pose advances over 0.3 seconds. It does
      // not teleport before contacts. Omitting seconds uses one fixed interval.
      e.moveKinematic({position:{x:1,y:0,z:0},rotation:{w:Math.cos(.2),x:0,y:Math.sin(.2),z:0}},.3);
      this.state.targetQueued=e.kinematicMotion?.control==='target';
    }
    if(this.state.steps===45){
      try{e.moveKinematic({position:{x:9,y:0,z:0},rotation:{w:0,x:0,y:0,z:0}});}catch(error){this.state.invalidRejected=true;}
      this.state.completed=Math.abs(e.transform.position.x-1)<.0001&&e.kinematicMotion?.control==='stopped';
    }
    // Readbacks are snapshots. Transform assignment remains explicit placement,
    // so continuous movement always uses a motion command in this example.
    this.state.actual=e.transform;this.state.velocity=e.velocity;this.state.angularVelocity=e.angularVelocity;
  }
}
