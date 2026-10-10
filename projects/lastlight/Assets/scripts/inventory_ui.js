import {itemTypes} from './items.js';

export const INVENTORY_ROWS=6;

// These field-card notes describe the existing game rules. They do not own item
// quantities, equipment or weapon state; Inventory remains the source of truth.
const fieldNotes={
 rifle:{use:'24-round magazine. Unlimited reserve ammunition.',controls:'1 select  |  LEFT MOUSE fire  |  R reload'},
 rpg:{use:'Uses your carried rockets. Keep the muzzle clear.',controls:'2 select  |  LEFT MOUSE fire'},
 framewalk_boots:{use:'Jump, then press SPACE again in the air toward a surface.',controls:'SPACE choose a surface  |  X release gravity frame'},
 rockets:{use:'Ammunition for the RPG launcher.',controls:'2 select launcher  |  LEFT MOUSE fire'},
 grenades:{use:'A thrown explosive. Keep clear of the blast.',controls:'G throw'}
};
// Registered project textures; generated field illustrations are presentation
// assets, independent of pickup prefabs and the data stored in the inventory.
const itemImages={
 rifle:'8d73a3680d7cfcbf848b87ed75425887',
 rpg:'987eb61bf8cafb5441005aff4673020c',
 framewalk_boots:'30786ec7e46418d579b59b98e24d4fa3',
 rockets:'1c0993c1fa14aabc91d6b8915033ad97',
 grenades:'d033d610b01dcf6d884aaca7748c4a5d'
};

// A small view over the player's inventory, using authored text/button elements.
// Selection and pagination are transient UI facts, separate from owned items.
export default class InventoryUI {
 constructor(player,document){this.player=player;this.doc=document;this.page=0;this.selected=null;this.rows=[];this.imageAsset=null;}
 render(){const inventory=this.player.inventory;if(!inventory)return;
  const entries=inventory.entries(),pages=Math.max(1,Math.ceil(entries.length/INVENTORY_ROWS));
  this.page=Math.min(this.page,pages-1);this.rows=entries.slice(this.page*INVENTORY_ROWS,(this.page+1)*INVENTORY_ROWS);
  if(!entries.some(e=>e.item===this.selected))this.selected=entries[0]?.item||null;
  const weapon=itemTypes[inventory.equipped('weapon')]?.name||'None';
  const feet=inventory.equipped('feet')?'Framewalk boots':'Ordinary shoes';
  this.doc.get('inventory_hint').text=`PACK ${inventory.state.slots.length} / ${inventory.state.capacity} slots  |  ${weapon}  |  ${feet}\nC collect an aimed pickup  /  I close pack. Ammunition has its own stock.`;
  for(let i=0;i<INVENTORY_ROWS;i++){const element=this.doc.get(`inventory_row_${i}`),entry=this.rows[i];
   this.doc.get(`inventory_row_selected_${i}`).visible=!!entry&&entry.item===this.selected;
   element.visible=!!entry;if(entry){const type=itemTypes[entry.item];
    const count=type.stock?`${entry.quantity} / ${type.maxStack}`:`x${entry.quantity}`;
    const status=entry.equipped?'  [EQUIPPED]':entry.quantity===0?'  [EMPTY]':'';
    element.enabled=true;element.text=`${entry.item===this.selected?'> ':'  '}${entry.name.toUpperCase()}  ${count}${status}`;}}
  const selected=entries.find(e=>e.item===this.selected),type=selected?itemTypes[selected.item]:null;
  const note=selected?fieldNotes[selected.item]:null;
  const status=type?.stock?`STOCK ${selected.quantity} / ${type.maxStack}`:
   selected?.equipped?`${type.slot.toUpperCase()} / EQUIPPED`:
   type?.slot?`${type.slot.toUpperCase()} / CARRIED`:'CARRIED';
  this.doc.get('inventory_item_title').text=selected?selected.name.toUpperCase():'PACK EMPTY';
  this.doc.get('inventory_item_status').text=selected?status:'NO ITEM SELECTED';
  this.doc.get('inventory_detail').text=selected?
   `${note?.use||type.description}\n\n${note?.controls||''}`:
   'Aim at a nearby pickup and press C.';
  const image=this.doc.get('inventory_item_image'),asset=selected?itemImages[selected.item]||'':'';
  image.visible=!!asset;
  if(this.imageAsset!==asset){image.texture=asset;this.imageAsset=asset;}
  const equip=this.doc.get('inventory_equip');equip.enabled=!!type?.slot&&selected.quantity>0&&!(type.permanent&&selected.equipped);
  equip.text=type?.stock?selected.item==='rockets'?'AMMUNITION STOCK':'SUPPLY STOCK':type?.permanent?selected.equipped?'WEAPON SELECTED':'SELECT WEAPON':selected?.equipped?'UNEQUIP':'EQUIP';
  this.doc.get('inventory_drop').enabled=!!selected&&inventory.canDrop(selected.item);
  this.doc.get('inventory_drop').text=!selected?'NO ITEM SELECTED':type?.permanent?'KEPT - CANNOT DROP':selected.quantity===0?'NO STOCK TO DROP':'DROP ONE';
  this.doc.get('inventory_previous').enabled=this.page>0;this.doc.get('inventory_next').enabled=this.page<pages-1;
  this.doc.get('inventory_page').text=`${this.page+1} / ${pages}`;
 }
 event(event){const inventory=this.player.inventory;
  if(event.type!=='click'||!inventory)return null;
  const row=/^inventory_row_(\d+)$/.exec(event.element);
  if(row){const entry=this.rows[Number(row[1])];if(entry)this.selected=entry.item;}
  else if(event.element==='inventory_previous'){this.page=Math.max(0,this.page-1);this.selected=inventory.entries()[this.page*INVENTORY_ROWS]?.item||null;}
  else if(event.element==='inventory_next'){this.page++;this.selected=inventory.entries()[this.page*INVENTORY_ROWS]?.item||null;}
  else if(event.element==='inventory_equip'&&this.selected){
   const type=itemTypes[this.selected];if(type?.slot){if(!type.permanent&&inventory.hasEquipment(this.selected))inventory.unequip(type.slot);else inventory.equip(this.selected);}
  }else if(event.element==='inventory_drop'&&this.selected&&inventory.queueDrop(this.selected))return 'drop';
  this.render();return null;
 }
}
