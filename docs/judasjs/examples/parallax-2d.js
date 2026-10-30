// Single-camera visual layer. Attach only to a body-free root sprite and assign
// target to the camera-followed actor. 0 = screen-locked, 1 = ordinary world speed.
export const properties={target:{type:'entity',default:null},factorX:{type:'number',default:.3},factorY:{type:'number',default:.15}};
export default class {
 /** @param {import('judas').ScriptContext<{target:import('judas').Entity|null;factorX:number;factorY:number}>} context */
 constructor({entity,properties}){this.entity=entity;this.properties=properties;
  /** @type {{origin:import('judas').Vec3|null,reference:import('judas').Vec3|null}} */
  this.state={origin:null,reference:null};}
 start(){
  if(this.entity.collider||this.entity.character||this.entity.parent||!this.properties.target?.valid)throw Error('Parallax needs a body-free sprite and a live camera target');
  if(!Number.isFinite(this.properties.factorX)||!Number.isFinite(this.properties.factorY))throw Error('Parallax factors must be finite');
  if(!this.state.origin){this.state.origin=this.entity.transform.position;this.state.reference=this.properties.target.transform.position;}
 }
 restore(){this.start();}
 presentationUpdate(){
  const target=this.properties.target;if(!target?.valid)return;
  const p=target.presentedTransform.position,o=this.state.origin,r=this.state.reference;
  if(!o||!r)throw Error('Parallax anchors were not initialized');
  this.entity.transform={position:{x:o.x+(p.x-r.x)*(1-this.properties.factorX),y:o.y+(p.y-r.y)*(1-this.properties.factorY),z:o.z}};
 }
}
