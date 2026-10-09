import {world,physics} from 'judas';
export default class {
 /** @param {{entity:import('judas').Entity}} context */
 constructor({entity}){this.entity=entity;this.state={spatial:false,uniform:false,field:false,spatialQuery:false,cleared:false,fixed:false};}
 start(){
  const g=this.entity.gravity;
  this.state.spatial=g.state.mode==='spatial';
  g.setUniform({x:3,y:0,z:0});
  this.state.uniform=Math.abs(g.acceleration.x-3)<.0001;
  const source=world.entity('2'); // Authored Gravity source in character_demo.
  if(source){g.select(source);this.state.field=g.state.mode==='field'&&g.state.available;}
  const p=this.entity.transform.position;
  this.state.spatialQuery=Number.isFinite(physics.gravity(p).y);
  g.clear();this.state.cleared=g.state.mode==='spatial';
 }
 fixedUpdate(){
  // A game may change selection here; this example leaves ordinary routing intact.
  this.state.fixed=true;
 }
}
