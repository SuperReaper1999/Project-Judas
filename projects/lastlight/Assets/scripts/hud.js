import {ui,input,scenes} from 'judas';
import {game} from './game.js';
import InventoryUI from './inventory_ui.js';

export default class HUD {
 constructor(player){
  this.player=player;this.doc=ui.get('lastlight_ui');this.shopping=false;this.inventoryOpen=false;
  this.inventoryView=new InventoryUI(player,this.doc);
 }
 start(){ui.debugOverlayVisible=false;this.menu(false);}
 menu(open){
  this.player.fireBlocked=true;this.shopping=false;this.inventoryOpen=false;
  this.doc.get('shop').visible=false;this.doc.get('inventory').visible=false;
  this.doc.modal=open;this.doc.get('pause').visible=open;input.pointerCapture=!open;
 }
 shop(open){
  this.player.fireBlocked=true;this.shopping=open;this.inventoryOpen=false;
  this.doc.get('pause').visible=false;this.doc.get('inventory').visible=false;
  this.doc.get('shop').visible=open;this.doc.modal=open;input.pointerCapture=!open;
 }
 inventory(open){
  this.player.fireBlocked=true;this.inventoryOpen=open;this.shopping=false;
  this.doc.get('pause').visible=false;this.doc.get('shop').visible=false;
  this.doc.get('inventory').visible=open;this.doc.modal=open;input.pointerCapture=!open;
  if(open)this.inventoryView.render();
 }
 update(){
  const ended=game.phase==='dead'||game.phase==='victory';
  if(input.pressed('pause')&&!ended){
   if(this.inventoryOpen)this.inventory(false);else if(this.shopping)this.shop(false);else this.menu(!this.doc.modal);
  }
  // UI update keeps I/Escape active while the inventory owns the modal pause.
  if(input.pressed('inventory')&&!ended)this.inventory(!this.inventoryOpen);
  if(ended){this.menu(true);this.doc.get('resume').enabled=false;
   this.doc.get('pause_title').text=game.phase==='victory'?'TOWN HELD!':'LASTLIGHT LOST';}
  const w=this.player.weapon,ammo=w.reload>0?w.reloadStage:w.mode==='rpg'?`${game.rockets} ROCKETS`:`${w.ammo} / 24`;
  this.doc.get('score').text=`HEALTH ${Math.ceil(game.health)} | SCORE ${game.score} | POINTS ${game.points} | ${w.mode==='rpg'?'RPG':'RIFLE'} ${ammo} | GRENADES ${game.grenades}`;
  const equipment=this.player.inventory?.hasEquipment('framewalk_boots')?' | FRAMEWALK BOOTS EQUIPPED':' | I: PACK / C: COLLECT';
  this.doc.get('progress').text=`WAVE ${game.wave} / 5 | ZOMBIES ${game.alive} | ${this.player.camera.third?'THIRD PERSON':'FIRST PERSON'}${this.player.grab?.held?' | CARRYING: B drop / T rotate':this.player.vendor?.near?' | N: SUPPLY STALL':''}${equipment}`;
  this.doc.get('message').text=game.phase==='breather'?`${Math.ceil(game.countdown)}s TO NEXT WAVE | ${game.message}`:game.message;
  this.doc.get('hitmark').visible=game.flash>0;this.doc.get('hurt').visible=game.hurt>0;
  if(this.shopping){
   this.doc.get('shop_hint').text=`${game.points} POINTS | Rockets ${game.rockets} | Grenades ${game.grenades} | Health ${Math.ceil(game.health)}`;
   this.doc.get('buy_rockets').enabled=game.points>=250&&game.rockets<=10;
   this.doc.get('buy_grenades').enabled=game.points>=150&&game.grenades<=10;
   this.doc.get('buy_heal').enabled=game.points>=100&&game.health<100;
  }
  if(this.inventoryOpen)this.inventoryView.render();
 }
 event(e){
  if(e.document!=='lastlight_ui')return;
  if(this.inventoryOpen){
   if(e.type==='back'||e.type==='click'&&e.element==='inventory_close')this.inventory(false);
   else if(this.inventoryView.event(e)==='drop')this.inventory(false);
   return;
  }
  if(this.shopping){
   if(e.type==='back'||e.type==='click'&&e.element==='shop_close')this.shop(false);
   else if(e.type==='click')this.player.vendor.buy(e.element);
   return;
  }
  if((e.type==='back'||e.type==='click'&&e.element==='resume')&&game.phase!=='dead'&&game.phase!=='victory')this.menu(false);
  if(e.type==='click'&&e.element==='restart')scenes.reload();
  if(e.type==='click'&&e.element==='quit')ui.quit();
 }
}
