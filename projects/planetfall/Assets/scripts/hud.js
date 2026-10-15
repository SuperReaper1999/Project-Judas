import {missions} from './campaign-core.js';
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
  this.player.fireBlocked=true;this.shopping=false;this.inventoryOpen=false;this.journalOpen=false;
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
  if(input.pressed('journal')&&!ended){const open=!this.journalOpen;this.menu(open);this.journalOpen=open;}
  if(input.pressed('inventory')&&!ended)this.inventory(!this.inventoryOpen);
  if(ended){this.menu(true);this.doc.get('resume').enabled=false;
   this.doc.get('pause_title').text=game.phase==='victory'?'LANTERN SAFE':'EXPEDITION LOST';}
  // Cards display existing game state; artwork never owns health or ammunition.
  const w=this.player.weapon,ammo=w.reload>0?w.reloadStage:w.mode==='rpg'?`${game.rockets} ROCKETS`:`${w.ammo} / 24`;
  this.doc.get('health').text=`HEALTH ${Math.ceil(game.health)}`;
  const width=Math.round(226*Math.max(0,Math.min(1,game.health/100)));
  if(width!==this.healthWidth){this.doc.get('health_fill').setLayout({size:{x:width,y:5}});this.healthWidth=width;}
  this.doc.get('wave').text=`${Math.min(20,(this.player.campaign?.state.mission||0)+1)} / 20`;
  this.doc.get('enemies').text=`${game.alive} ENEMIES`;
  this.doc.get('score').text=`SCORE ${game.score}`;
  this.doc.get('points').text=`${game.points} POINTS`;
  this.doc.get('weapon_image').visible=!this.player.ship?.piloting;
  this.doc.get('weapon').text=this.player.ship?.piloting?'WAYFARER':w.mode==='rpg'?'RPG LAUNCHER':'RIFLE';
  this.doc.get('ammo').text=this.player.ship?.piloting?'FLIGHT ASSIST ON':ammo;
  this.doc.get('grenades').text=`GRENADES ${game.grenades}`;
  const art=w.mode==='rpg'?'987eb61bf8cafb5441005aff4673020c':'8d73a3680d7cfcbf848b87ed75425887';
  if(art!==this.weaponArt){this.doc.get('weapon_image').texture=art;this.weaponArt=art;}
  const equipment=this.player.inventory?.hasEquipment('framewalk_boots')?' / FRAMEWALK':'';
  this.doc.get('pause_controls').text=this.player.ship?.piloting?'WASD forward / strafe\nMOUSE steer / V chase view\nSPACE rise / CTRL descend\nSHIFT boost / R auto level\nRelease controls to brake/hover\nT choose planet / N cruise + land\nE leave chair after stopping\nWalk to orange door / E open': 'WASD move / SHIFT faster\nSPACE jump / E mantle or ship use\nLEFT MOUSE fire / R reload\nRIGHT MOUSE punch / F heavy / Q shove\n1 rifle / 2 launcher / G grenade\nB carry / T rotate / C collect\nI pack / N supplies / V view\nX release gravity / E ship door, chair';
  this.doc.get('progress').text=this.player.ship?.piloting?`WAYFARER / ${this.player.camera.third?'CHASE VIEW':'COCKPIT'}`:`${this.player.camera.third?'THIRD PERSON':'FIRST PERSON'}${equipment} / SHIP ${Math.round(Math.hypot(this.player.entity.transform.position.x-this.player.ship.hull.transform.position.x,this.player.entity.transform.position.y-this.player.ship.hull.transform.position.y,this.player.entity.transform.position.z-this.player.ship.hull.transform.position.z))}m`;
  this.doc.get('controls').text=this.player.ship?.prompt|| (this.player.grab?.held?'B  DROP     T  ROTATE     I  PACK':
   this.player.vendor?.near?'N  SUPPLY STALL     I  PACK     ESC  PAUSE':'I  PACK     V  VIEW     ESC  PAUSE');
  if(this.journalOpen){const c=this.player.campaign.state,m=missions[c.mission];this.doc.get('pause_title').text=m?`${m.id}. ${m.title}`:'The Fracture / Epilogue';this.doc.get('pause_hint').text=m?(this.player.campaign.dialogue||m.brief):'Your choice: '+c.choice;this.doc.get('pause_controls').text=`${m?.npc||'Lantern'}\n${m?.region||'EARTH'} / ${c.progress}/${m?.need||1}\n`+Object.entries(c.resources).map(([k,v])=>k.toUpperCase()+': '+v).join('\n')+`\nRATIONS ${c.food} / HUNGER ${Math.round(c.hunger)}\nH objective / M mine / C salvage\nP side contracts / F5 save / F9 load\nFinal council: H contain / SHIFT+H reconnect`;}else this.doc.get('pause_title').text='PLANETFALL / PAUSED';
  this.doc.get('message').text=this.player.campaign.saveNotice||game.message;
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
  if(e.type==='click'&&e.element==='restart'){const c=this.player.campaign;if(game.health<=0){c.state.health=100;game.health=100;game.points=Math.floor(game.points*.9);c.state.resources.credits=game.points;}c.publish();scenes.reload();}
  if(e.type==='click'&&e.element==='quit')ui.quit();
 }
}
