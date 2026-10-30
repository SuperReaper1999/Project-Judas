export const properties={spin:{type:'boolean',default:false}};
export default class {
 constructor({entity,properties}){this.entity=entity;this.properties=properties;this.state={time:0};}
 fixedUpdate(dt){
   this.state.time+=dt;const q=this.entity.transform.rotation,n=this.entity.body2D.normal;
   const right={x:1-2*(q.y*q.y+q.z*q.z),y:2*(q.x*q.y+q.w*q.z),z:2*(q.x*q.z-q.w*q.y)};
   const speed=this.properties.spin?0:1.2*Math.cos(this.state.time*.7),spin=this.properties.spin ? .14*Math.cos(this.state.time*.6):0;
   this.entity.setKinematicVelocity({x:right.x*speed,y:right.y*speed,z:right.z*speed},{x:n.x*spin,y:n.y*spin,z:n.z*spin});
 }
}