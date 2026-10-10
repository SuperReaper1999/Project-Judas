import {itemTypes} from './items.js';

export const INVENTORY_ROWS=6;

// A small view over the player's inventory, using authored text/button elements.
// Selection and pagination are transient UI facts, separate from owned items.
export default class InventoryUI {
 constructor(player,document){this.player=player;this.doc=document;this.page=0;this.selected=null;this.rows=[];}
 render(){const inventory=this.player.inventory;if(!inventory)return;
  const entries=inventory.entries(),pages=Math.max(1,Math.ceil(entries.length/INVENTORY_ROWS));
  this.page=Math.min(this.page,pages-1);this.rows=entries.slice(this.page*INVENTORY_ROWS,(this.page+1)*INVENTORY_ROWS);
  if(!entries.some(e=>e.item===this.selected))this.selected=entries[0]?.item||null;
  this.doc.get('inventory_hint').text=`PACK ${inventory.state.slots.length} / ${inventory.state.capacity} | WEAPON: ${inventory.equipped('weapon').toUpperCase()} | FEET: ${inventory.equipped('feet')?'FRAMEWALK BOOTS':'ordinary shoes'}\nC collects aimed items. Boots begin unequipped; choose an item below.`;
  for(let i=0;i<INVENTORY_ROWS;i++){const element=this.doc.get(`inventory_row_${i}`),entry=this.rows[i];
   element.visible=!!entry;if(entry){element.enabled=true;element.text=`${entry.item===this.selected?'> ':''}${entry.name} x${entry.quantity}${entry.equipped?' [ON]':''}`;}}
  const selected=entries.find(e=>e.item===this.selected),type=selected?itemTypes[selected.item]:null;
  this.doc.get('inventory_detail').text=selected?`${selected.name}\n${type.description}`:'The pack is empty. Aim at a pickup and press C.';
  const equip=this.doc.get('inventory_equip');equip.enabled=!!type?.slot&&selected.quantity>0&&!(type.permanent&&selected.equipped);
  equip.text=type?.permanent?selected.equipped?'SELECTED WEAPON':'SELECT WEAPON':selected?.equipped?'UNEQUIP':'EQUIP SELECTED';
  this.doc.get('inventory_drop').enabled=!!selected&&inventory.canDrop(selected.item);
  this.doc.get('inventory_drop').text='DROP ONE INTO THE TOWN';
  this.doc.get('inventory_previous').enabled=this.page>0;this.doc.get('inventory_next').enabled=this.page<pages-1;
  this.doc.get('inventory_page').text=`${this.page+1} / ${pages}`;
 }
 event(event){const inventory=this.player.inventory;
  if(event.type!=='click'||!inventory)return null;
  const row=/^inventory_row_(\d+)$/.exec(event.element);
  if(row){const entry=this.rows[Number(row[1])];if(entry)this.selected=entry.item;}
  else if(event.element==='inventory_previous')this.page=Math.max(0,this.page-1);
  else if(event.element==='inventory_next')this.page++;
  else if(event.element==='inventory_equip'&&this.selected){
   const type=itemTypes[this.selected];if(type?.slot){if(!type.permanent&&inventory.hasEquipment(this.selected))inventory.unequip(type.slot);else inventory.equip(this.selected);}
  }else if(event.element==='inventory_drop'&&this.selected&&inventory.queueDrop(this.selected))return 'drop';
  this.render();return null;
 }
}
