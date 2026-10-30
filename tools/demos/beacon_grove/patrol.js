// A normal kinematic sensor. Velocity is integrated by native planar physics.
export default class {
 constructor({entity}){this.entity=entity;this.state={time:0};}
 fixedUpdate(dt){this.state.time+=dt;
  this.entity.setKinematicVelocity({x:1.5*Math.cos(this.state.time),y:0,z:0},{x:0,y:0,z:0});
 }
}
