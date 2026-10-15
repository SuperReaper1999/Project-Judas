import {world} from 'judas';
// Scene-local ownership for transient mapped bodies. No behaviour instances are
// shared. Body/entity IDs are retired when their owning articulation ends.
const boneOwners=new Map();
const corpses=new Map();
// Every new death is physical. Retire the oldest whole articulation rather
// than making later deaths fake, or leaving unbounded sleeping bodies behind.
export const corpseLimit=8;
export const activeTrips=new Set();
export const tripBones=['pelvis','chest','head','upper_arm.L','forearm.L','upper_arm.R','forearm.R','thigh.L','shin.L','thigh.R','shin.R'];
export function registerBones(owner,ragdoll){for(const key of tripBones){const b=ragdoll.body(key);if(b?.valid)boneOwners.set(b.id,owner.id);}}
export function registerCorpse(owner){
 activeTrips.delete(owner.id);
 for(const [id,entity] of corpses)if(!entity.valid)corpses.delete(id);
 corpses.set(owner.id,owner);
 while(corpses.size>corpseLimit){const [id,oldest]=corpses.entries().next().value;corpses.delete(id);if(oldest.valid)oldest.destroy();}
}
export function retireBones(owner){for(const [body,id] of boneOwners)if(id===owner.id)boneOwners.delete(body);activeTrips.delete(owner.id);corpses.delete(owner.id);}
export function targetOf(entity){if(!entity?.valid)return null;
 const id=boneOwners.get(entity.id);if(id){const owner=world.entity(id);return owner?.valid?owner:null;}
 return entity.hasTag('enemy')?entity:null;
}
