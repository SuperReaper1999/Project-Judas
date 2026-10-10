import {ui,input,scenes} from 'judas';
import {game} from './game.js';
import InventoryUI from './inventory_ui.js';

export default class HUD {
 constructor(player){
  this.player=player;this.doc=ui.get('lastlight_ui');this.shopping=false;this.inventoryOpen=false;
  this.inventoryView=new InventoryUI(player,this.doc);
  this.healthWidth=-1;this.weaponArt='';
 }
 start(){ui.debugOverlayVisible=false;this.menu(false);}
 menu(open){
  this.player.fireBlocked=true;this.shopping=false;this.inventoryOpen=false;
  this.doc.get('shop').visible=false;this.doc.get('inventory').visible=false;
  this.doc.modal=open;this.doc.get('pause').visible=open;this.doc.get('modal_shade').visible=open;input.pointerCapture=!open;
 }
 shop(open){
  this.player.fireBlocked=true;this.shopping=open;this.inventoryOpen=false;
  this.doc.get('pause').visible=false;this.doc.get('inventory').visible=false;
  this.doc.get('shop').visible=open;this.doc.get('modal_shade').visible=open;this.doc.modal=open;input.pointerCapture=!open;
 }
 inventory(open){
  this.player.fireBlocked=true;this.inventoryOpen=open;this.shopping=false;
  this.doc.get('pause').visible=false;this.doc.get('shop').visible=false;
  this.doc.get('inventory').visible=open;this.doc.get('modal_shade').visible=open;this.doc.modal=open;input.pointerCapture=!open;
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
  // Cards display existing game state; artwork never owns health or ammunition.
  const w=this.player.weapon,ammo=w.reload>0?w.reloadStage:w.mode==='rpg'?`${game.rockets} ROCKETS`:`${w.ammo} / 24`;
  this.doc.get('health').text=`HEALTH ${Math.ceil(game.health)}`;
  const width=Math.round(226*Math.max(0,Math.min(1,game.health/100)));
  if(width!==this.healthWidth){this.doc.get('health_fill').setLayout({size:{x:width,y:5}});this.healthWidth=width;}
  this.doc.get('wave').text=`${String(game.wave).padStart(2,'0')} / 05`;
  this.doc.get('enemies').text=`${game.alive} ENEMIES`;
  this.doc.get('score').text=`SCORE ${game.score}`;
  this.doc.get('points').text=`${game.points} POINTS`;
  this.doc.get('weapon').text=w.mode==='rpg'?'RPG LAUNCHER':'RIFLE';
  this.doc.get('ammo').text=ammo;
  this.doc.get('grenades').text=`GRENADES ${game.grenades}`;
  const art=w.mode==='rpg'?'987eb61bf8cafb5441005aff4673020c':'8d73a3680d7cfcbf848b87ed75425887';
  if(art!==this.weaponArt){this.doc.get('weapon_image').texture=art;this.weaponArt=art;}
  const equipment=this.player.inventory?.hasEquipment('framewalk_boots')?' / FRAMEWALK':'';
  this.doc.get('progress').text=`${this.player.camera.third?'THIRD PERSON':'FIRST PERSON'}${equipment}`;
  this.doc.get('controls').text=this.player.grab?.held?'B  DROP     T  ROTATE     I  PACK':
   this.player.vendor?.near?'N  SUPPLY STALL     I  PACK     ESC  PAUSE':'I  PACK     V  VIEW     ESC  PAUSE';
  this.doc.get('message').text=game.phase==='breather'?`${Math.ceil(game.countdown)}s TO NEXT WAVE | ${game.message}`:game.message;
  this.doc.get('message_backing').visible=!!this.doc.get('message').text;
  this.doc.get('hitmark').visible=game.flash>0;this.doc.get('hurt').visible=game.hurt>0;
  if(this.shopping){
   this.doc.get('shop_hint').text=`${game.points} POINTS TO SPEND\nRockets ${game.rockets} / 12   |   Grenades ${game.grenades} / 12   |   Health ${Math.ceil(game.health)} / 100`;
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
