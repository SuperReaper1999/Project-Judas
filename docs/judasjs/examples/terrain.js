// Attach to an authored Terrain V2 owner (terrain.asset), root/unit-scale/static.
// Create its landscape with the editor Terrain tool before attaching this script.
import {input,physics,world} from 'judas';
export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}){this.entity=entity;}
  fixedUpdate(){
    const terrain=this.entity.terrain,state=terrain.state;
    if(!state?.ready||state.busy||!input.pressed('dig'))return;
    const ray=world.viewRay;if(!ray)return;
    const hit=physics.raycast(ray.origin,ray.direction,10);
    if(hit?.entity?.id===this.entity.id)terrain.brush(hit.point,{kind:'remove',radius:1.8});
  }
}
