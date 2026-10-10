// Item meaning belongs to the project. Prefabs merely provide ordinary bodies,
// geometry and this small, inspectable pickup description on their root slot.
export const itemTypes=Object.assign(Object.create(null),{
 rifle:{name:'Rifle',slot:'weapon',maxStack:1,permanent:true,
  description:'Starting rifle. Select here or press 1. Its magazine and reload stay on the existing weapon; reserve rifle ammunition is unlimited.'},
 rpg:{name:'RPG launcher',slot:'weapon',maxStack:1,permanent:true,
  description:'Starting launcher. Select here or press 2, then fire purchased or collected rockets. This selects the existing RPG.'},
 framewalk_boots:{name:'Judas Framewalk Boots',slot:'feet',maxStack:1,
  prefabProperty:'framewalkBoots',description:'Choose a real nearby surface to walk on its walls and ceiling. Equip to enable; unequip to return to ordinary gravity.'},
 rockets:{name:'Rockets',stock:'rockets',maxStack:12,prefabProperty:'rocketsPickup',
  description:'Shared with the supply stall and RPG. Press 2, then fire. The inventory and weapon use the same rocket counter.'},
 grenades:{name:'Grenades',stock:'grenades',maxStack:12,prefabProperty:'grenadesPickup',
  description:'Shared with the supply stall. Press G to throw. The inventory and throw action use the same grenade counter.'}
});
export const equipmentSlots=['weapon','feet','body','hands','accessory'];
export const properties={item:{type:'string',default:'framewalk_boots'},
 quantity:{type:'number',default:1},prefab:{type:'string',default:''}};

export default class ItemPickup {
 constructor({entity,properties}){
  this.entity=entity;
  const item=itemTypes[properties.item];
  this.state={pickup:!!item,item:item?properties.item:'',
   quantity:item?Math.max(1,Math.min(item.maxStack,Math.floor(properties.quantity))):0,
   prefab:properties.prefab};
 }
 // No proximity trigger awards items automatically. The player's collect cast
 // must see this actual solid body, and a full pack leaves the pickup in place.
}
