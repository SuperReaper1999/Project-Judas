import {input,physics} from 'judas';

export const properties={maximum:{type:'number',default:30},spread:{type:'number',default:.35},height:{type:'number',default:2}};

/** @param {import('judas').Quat} q @param {import('judas').Vec3} v */
function rotate(q,v) {
  const t={x:2*(q.y*v.z-q.z*v.y),y:2*(q.z*v.x-q.x*v.z),z:2*(q.x*v.y-q.y*v.x)};
  return {x:v.x+q.w*t.x+q.y*t.z-q.z*t.y,
    y:v.y+q.w*t.y+q.z*t.x-q.x*t.z,z:v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}

/** A 10x10 fan in the owner's local forward/up/right frame, not universal up.
 * @param {import('judas').Transform} transform
 * @param {{maximum:number,spread:number,height:number}} settings
 * @returns {import('judas').RaycastRequest[]}
 */
export function makeRayFan(transform,settings) {
  const forward=rotate(transform.rotation,{x:0,y:0,z:-1});
  const right=rotate(transform.rotation,{x:1,y:0,z:0});
  const up=rotate(transform.rotation,{x:0,y:1,z:0});
  const origin={x:transform.position.x+up.x*settings.height,
    y:transform.position.y+up.y*settings.height,z:transform.position.z+up.z*settings.height};
  const rays=[];
  for(let row=0;row<10;++row)for(let column=0;column<10;++column) {
    const x=(column/9*2-1)*settings.spread,y=(row/9*2-1)*settings.spread;
    rays.push({origin,direction:{x:forward.x+right.x*x+up.x*y,
      y:forward.y+right.y*x+up.y*y,z:forward.z+right.z*x+up.z*y},maximum:settings.maximum});
  }
  return rays;
}

/** @param {import('judas').CastHit|null} a @param {import('judas').CastHit|null} b */
function sameHit(a,b) {
  if(!a||!b)return a===b;
  /** @param {number} x @param {number} y */
  const close=(x,y)=>Math.abs(x-y)<1e-5;
  return a.entityId===b.entityId&&a.bodyId===b.bodyId&&a.primitiveIndex===b.primitiveIndex&&
    a.childKey===b.childKey&&a.feature===b.feature&&a.shape===b.shape&&a.initialOverlap===b.initialOverlap&&
    close(a.distance,b.distance)&&close(a.fraction,b.fraction)&&
    close(a.point.x,b.point.x)&&close(a.point.y,b.point.y)&&close(a.point.z,b.point.z)&&
    close(a.normal.x,b.normal.x)&&close(a.normal.y,b.normal.y)&&close(a.normal.z,b.normal.z);
}

export default class {
  /** @param {import('judas').ScriptContext<{maximum:number,spread:number,height:number}>} context */
  constructor({entity,properties}) {
    this.entity=entity;this.settings=properties;
    this.state={count:0,hits:0,matched:false,comparisons:0,
      results:/** @type {(null|{entity:string,distance:number,point:import('judas').Vec3,normal:import('judas').Vec3})[]} */([])};
  }
  fixedUpdate() {
    const rays=makeRayFan(this.entity.transform,this.settings);
    const filter={ignored:[this.entity]};
    const hits=physics.raycastMany(rays,filter); // ONE bridge call, still 100 queries.
    this.state.count=hits.length;
    this.state.hits=hits.filter(Boolean).length;
    this.state.results=hits.map(hit=>hit?{entity:hit.entityId,distance:hit.distance,point:hit.point,normal:hit.normal}:null);
    if(input.pressed('compare_rays')) {
      const scalar=rays.map(ray=>physics.raycast(ray.origin,ray.direction,ray.maximum,filter));
      this.state.matched=hits.every((hit,index)=>sameHit(hit,scalar[index]));
      ++this.state.comparisons;
    }
    // Selecting a ledge or starting a climb from these observations is game JS.
  }
}
