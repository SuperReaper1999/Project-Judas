import {input,world,physics} from 'judas';
import {game} from './game.js';
import {itemTypes,equipmentSlots} from './items.js';
import {add,sub,mul,length,norm} from './math.js';

const CAPACITY=8;
const integer=n=>Number.isFinite(n)&&Math.floor(n)===n&&n>0;

// This reusable project helper stores no Entity/UI handles in player.state.
// Pack entries are bounded plain data; slot equipment names an owned item type.
// Explosive quantities stay exclusively in game.rockets/game.grenades, so vendor
// purchases and weapon use cannot create an independent inventory ammunition pool.
export default class Inventory {
 constructor(player){
  this.player=player;
  if(!player.state.inventory)player.state.inventory={capacity:CAPACITY,
   slots:[{item:'rifle',quantity:1,prefab:''},{item:'rpg',quantity:1,prefab:''}],
   equipment:{weapon:'rifle',feet:null,body:null,hands:null,accessory:null},
   stockPrefabs:{rockets:'',grenades:''},pendingDrop:null};
 }
 get state(){return this.player.state.inventory;}
 count(key){const type=itemTypes[key];if(!type)return 0;
  if(type.stock)return game[type.stock];
  return this.state.slots.reduce((n,s)=>n+(s.item===key?s.quantity:0),0);
 }
 equipped(slot){
  // Number keys and this menu select the same existing Weapon helper. The plain
  // weapon slot is a published view of that current mode, not a second weapon.
  if(slot==='weapon'&&this.player.weapon)this.state.equipment.weapon=this.player.weapon.mode;
  return equipmentSlots.includes(slot)?this.state.equipment[slot]:null;
 }
 hasEquipment(key){const type=itemTypes[key];return !!type?.slot&&this.equipped(type.slot)===key;}
 accepts(key,quantity=1){const type=itemTypes[key];if(!type||!integer(quantity)||quantity>type.maxStack)return false;
  if(type.permanent&&this.count(key)>0)return false;
  if(type.stock)return this.count(key)+quantity<=type.maxStack;
  const space=this.state.slots.reduce((n,s)=>n+(s.item===key?type.maxStack-s.quantity:0),0);
  return quantity<=space+(CAPACITY-this.state.slots.length)*type.maxStack;
 }
 add(key,quantity=1,prefab=''){
  if(!this.accepts(key,quantity))return false;const type=itemTypes[key];
  if(type.stock){game[type.stock]+=quantity;if(prefab)this.state.stockPrefabs[key]=prefab;return true;}
  let remaining=quantity;
  for(const entry of this.state.slots){if(entry.item!==key)continue;
   const amount=Math.min(remaining,type.maxStack-entry.quantity);entry.quantity+=amount;remaining-=amount;
   if(remaining===0)return true;
  }
  while(remaining>0){const amount=Math.min(remaining,type.maxStack);
   this.state.slots.push({item:key,quantity:amount,prefab});remaining-=amount;}
  return true;
 }
 remove(key,quantity=1){if(!integer(quantity)||this.count(key)<quantity)return false;const type=itemTypes[key];
  if(type.permanent)return false;
  if(type.stock){game[type.stock]-=quantity;return true;}
  let remaining=quantity;
  for(let i=this.state.slots.length-1;i>=0;i--){const entry=this.state.slots[i];if(entry.item!==key)continue;
   const amount=Math.min(remaining,entry.quantity);entry.quantity-=amount;remaining-=amount;
   if(entry.quantity===0)this.state.slots.splice(i,1);if(remaining===0)break;
  }
  if(this.count(key)===0&&type.slot&&this.equipped(type.slot)===key)this.state.equipment[type.slot]=null;
  return true;
 }
 equip(key){const type=itemTypes[key];if(!type?.slot||this.count(key)<1)return false;
  if(type.slot==='weapon')this.player.weapon.select(key);
  this.state.equipment[type.slot]=key;game.message=`${type.name.toUpperCase()} EQUIPPED`;return true;
 }
 unequip(slot){if(slot==='weapon'||!equipmentSlots.includes(slot)||!this.state.equipment[slot])return false;
  const name=itemTypes[this.state.equipment[slot]].name;this.state.equipment[slot]=null;
  game.message=`${name.toUpperCase()} UNEQUIPPED${slot==='feet'?' - ordinary gravity':''}`;return true;
 }
 entries(){
  // These detached view rows may combine pack stacks. They are presentation,
  // never another stored quantity or a second source of ammunition truth.
  const keys=[];for(const entry of this.state.slots)if(!keys.includes(entry.item))keys.push(entry.item);
  keys.push('rockets','grenades');
  return keys.map(key=>({item:key,name:itemTypes[key].name,quantity:this.count(key),
   equipped:this.hasEquipment(key),slot:itemTypes[key].slot||null}));
 }
 prefab(key){const type=itemTypes[key];if(!type)return '';
  return this.player.props[type.prefabProperty]||
   (type.stock?this.state.stockPrefabs[key]:this.state.slots.find(s=>s.item===key)?.prefab)||'';
 }
 canDrop(key){return !itemTypes[key]?.permanent&&this.count(key)>0&&!!this.prefab(key)&&!this.state.pendingDrop;}
 queueDrop(key){if(!this.canDrop(key))return false;this.state.pendingDrop={item:key};return true;}
 drop(key){const type=itemTypes[key];if(!type||type.permanent||this.count(key)<1)return false;
  const asset=this.prefab(key);if(!asset)return false;
  const view=this.player.camera.pose(),filter={ignored:[this.player.entity]},direction=norm(view.direction);
  // Check the same ordinary space in which the new finite-mass pickup will be
  // placed. A blocked drop keeps the item and equipment untouched in the pack.
  if(physics.sphereCast(view.eye,.6,direction,1.25,filter)){
   game.message='DROP BLOCKED - face a clear space and try again';return false;
  }
  const e=world.spawnPrefab(asset,{position:add(view.eye,mul(direction,1.25)),rotation:view.rotation},
   {velocity:add(this.player.entity.character.velocity,mul(direction,1.2)),
    scripts:[{slot:1,properties:{item:key,quantity:1,prefab:asset}}]});
  if(!e?.valid)return false;
  this.remove(key,1);
  // Dropping the equipped pair is one equipment change. The gravity mechanic
  // observes it on this fixed step and owns clearing its real gravity override.
  if(type.slot&&this.equipped(type.slot)===key)this.state.equipment[type.slot]=null;
  game.message=`DROPPED ${type.name.toUpperCase()} - C to collect`;return true;
 }
 collect(){
  const view=this.player.camera.pose(),filter={ignored:[this.player.entity]};
  const sight=physics.raycast(view.position,view.direction,4.5,filter);
  const target=sight?sight.point:add(view.eye,mul(view.direction,3.2)),direction=norm(sub(target,view.eye));
  if(length(direction)<.001)return false;
  // A nearest solid cast from the actual eye prevents a third-person camera
  // seeing an item round a corner from collecting it through ordinary cover.
  const hit=physics.raycast(view.eye,direction,3.2,filter),entity=hit?.entity;
  if(!entity?.valid||!entity.hasTag('inventory_item')){
   game.message='C: aim at a nearby item to collect it';return false;
  }
  const item=entity.scriptState(1);
  if(!item?.pickup||!itemTypes[item.item]||!integer(item.quantity))return false;
  if(!this.accepts(item.item,item.quantity)){
   game.message=itemTypes[item.item].stock?'AMMUNITION FULL - room needed for the whole pickup':'PACK FULL - drop an item with I';return false;
  }
  // Destroy first only after the bounded transfer has been accepted. Failure
  // to destroy leaves storage untouched; ordinary reload recreates authored loot.
  if(!entity.destroy())return false;
  this.add(item.item,item.quantity,item.prefab);
  game.message=`COLLECTED ${itemTypes[item.item].name.toUpperCase()}${item.quantity>1?` x${item.quantity}`:''}${itemTypes[item.item].slot?' - I to equip':''}`;
  return true;
 }
 tick(){
  this.equipped('weapon');
  // UI may pause fixed simulation. Its drop button records plain intent and
  // closes the menu; spawning is done here, once normal physics has resumed.
  if(this.state.pendingDrop){const pending=this.state.pendingDrop;this.state.pendingDrop=null;this.drop(pending.item);}
  if(input.pressed('collect')&&!this.player.hud.doc.modal&&!this.player.climb.active&&!this.player.melee.busy&&!this.player.grab.held)this.collect();
 }
}
