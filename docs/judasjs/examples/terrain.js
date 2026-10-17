// Attach to a root unit-scale static body. Game meaning remains project-owned.
import {input,physics,world} from 'judas';
export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}){this.entity=entity;}
  ensureCreated(){if(!this.entity.terrain.state)this.entity.terrain.create({
    resolution:{x:21,y:17,z:21},extent:{x:16,y:10,z:16},navigation:true
  });}
  fixedUpdate(){
    this.ensureCreated();
    const terrain=this.entity.terrain,state=terrain.state;
    if(!state?.ready||state.busy||!input.pressed('dig'))return;
    const ray=world.viewRay;if(!ray)return;
    const hit=physics.raycast(ray.origin,ray.direction,10);
    if(hit?.entity?.id===this.entity.id)terrain.brush(hit.point,{kind:'remove',radius:1.8});
  }
}
