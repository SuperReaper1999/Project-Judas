import {destinations} from './expedition.js';
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
  this.player.fireBlocked=true;this.shopping=false;this.inventoryOpen=false;this.journalOpen=false;this.pageName=null;this.doc.get('field_menu').visible=false;
  this.doc.get('shop').visible=false;this.doc.get('inventory').visible=false;
  this.doc.modal=open;this.doc.get('pause').visible=open;this.doc.get('modal_shade').visible=open;input.pointerCapture=!open;
 }
 shop(open){
  this.player.fireBlocked=true;this.shopping=open;this.inventoryOpen=false;
  this.doc.get('field_menu').visible=false;this.pageName=null;this.doc.get('pause').visible=false;this.doc.get('inventory').visible=false;
  this.doc.get('shop').visible=open;this.doc.get('modal_shade').visible=open;this.doc.modal=open;input.pointerCapture=!open;
 }
 inventory(open){
  this.player.fireBlocked=true;this.inventoryOpen=open;this.shopping=false;
  this.doc.get('field_menu').visible=false;this.pageName=open?'pack':null;this.doc.get('pause').visible=false;this.doc.get('shop').visible=false;
  this.doc.get('inventory').visible=open;this.doc.get('modal_shade').visible=open;this.doc.modal=open;input.pointerCapture=!open;
  if(open)this.inventoryView.render();
 }
 page(name){if(name==='pack'){this.inventory(true);return;}this.menu(false);this.pageName=name;this.doc.modal=true;this.doc.get('field_menu').visible=true;this.doc.get('modal_shade').visible=true;input.pointerCapture=false;this.renderPage();}
 renderPage(){const c=this.player.campaign,s=c.state,m=missions[s.mission],name=this.pageName;this.actions=[];
  const action=(text,run,enabled=true)=>this.actions.push({text,run,enabled});
  let title='',summary='',body='';
  if(name==='journal'){title=m?m.id+'. '+m.title:'The Fracture / Epilogue';summary=m?m.brief:'Your choice: '+s.choice;
   body=(c.dialogue||'Find the gold marker. E shows the available interaction.')+'\n\n'+(m?m.npc+' / '+m.region.toUpperCase()+' / '+s.progress+' of '+m.need:'Campaign complete')+'\n\n'+Object.entries(s.resources).map(([k,v])=>k.toUpperCase()+': '+Math.floor(v)).join('  |  ')+'\nRations: '+s.food+' / Hunger: '+Math.round(s.hunger);
   action('Eat ration (+health)',()=>this.command('eat'),s.food>0);
   action('Next side contract',()=>this.command('contract'));
   action('Upgrade nearby rover',()=>this.command('upgrade_rover'),!!this.player.rover?.entity?.valid&&!s.built.includes('rover')&&Math.hypot(this.player.rover.entity.transform.position.x-this.player.entity.transform.position.x,this.player.rover.entity.transform.position.y-this.player.entity.transform.position.y,this.player.rover.entity.transform.position.z-this.player.entity.transform.position.z)<3.5);
   if(m?.kind==='choice'&&this.player.interaction.context()?.kind==='council'){action('Contain the crystal',()=>{this.player.interaction.choice='contain';this.command('quest');});action('Reconnect the network',()=>{this.player.interaction.choice='reconnect';this.command('quest');});}
  }else if(name==='map'){title='Flight chart';summary='Detailed regions connect through Wayfarer travel. Board the command chair to depart.';
   body='CURRENT REGION: '+c.region.toUpperCase()+'\n\n'+destinations.map((d,i)=>(i===this.player.ship.destination?'> ':'  ')+d.name).join('\n')+'\n\nSelected: '+destinations[this.player.ship.destination].name+'\nInterplanetary travel requires a repaired drive and fuel.';
   action('Previous destination',()=>{this.player.ship.destination=(this.player.ship.destination+destinations.length-1)%destinations.length;this.renderPage();});
   action('Next destination',()=>{this.player.ship.destination=(this.player.ship.destination+1)%destinations.length;this.renderPage();});
   action('Depart / auto-land',()=>this.command('travel'),this.player.ship.piloting);
  }else{title='Controls / contextual actions';summary='Move, look, interact. Other systems are available through this menu.';
   body=this.player.ship.piloting?'PILOTING\nWASD thrust / mouse steer\nSPACE rise / RIGHT MOUSE descend\nSHIFT boost / R level / V view\nRelease controls to hover. E leaves chair.\nChoose destinations in Map / Travel.':this.player.rover.active?'DRIVING\nWASD drive / steer\nSPACE brake / E leave rover': 'ON FOOT\nWASD move / SHIFT faster\nSPACE jump; mantle when facing a low ledge\nE interact with the prompted object\nLEFT MOUSE shoot / RIGHT MOUSE aim\nR reload / F melee / Q shove / G grenade\nV first / third person\nEquipped boots: airborne SPACE switches gravity.';
   action('Release gravity boots',()=>this.command('release'));
   action('Rotate carried object',()=>this.command('rotate'),!!this.player.grab.held);
  }
  this.doc.get('field_title').text=title;this.doc.get('field_summary').text=summary;this.doc.get('field_body').text=body;
  for(let i=0;i<7;i++){const a=this.actions[i],e=this.doc.get('field_action_'+i);e.visible=!!a;if(a){e.text=a.text;e.enabled=a.enabled;}}
 }
 command(kind){this.player.interaction.queue(kind);this.menu(false);}
 update(dt=.016){
  const ended=game.phase==='dead'||game.phase==='victory';
  if(input.pressed('pause')&&!ended){
   if(this.pageName||this.inventoryOpen)this.menu(false);else if(this.shopping)this.shop(false);else this.menu(!this.doc.modal);
  }
  if(input.pressed('inventory')&&!ended){if(this.doc.modal&&!this.shopping)this.menu(false);else this.page('journal');}
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
  this.doc.get('pause_controls').text='WASD move / SHIFT faster\nSPACE jump or mantle\nE interact (see prompt)\nLEFT MOUSE fire / RIGHT MOUSE aim\nR reload / F melee / Q shove\nG grenade / V camera\nTAB equipment, journal and travel\nESC pause';
  this.doc.get('progress').text=this.player.ship?.piloting?'WAYFARER / '+this.player.ship.status:this.player.rover?.active?'ROVER / SPACE brake':`${this.player.camera.third?'THIRD PERSON':'FIRST PERSON'}${equipment}`;
  const context=this.player.interaction?.context();
  this.doc.get('controls').text=(context?'E  '+context.label+'    |    ':'')+'TAB  Expedition    ESC  Pause';
  this.doc.get('pause_title').text=ended?'EXPEDITION LOST':'PLANETFALL / PAUSED';
  this.doc.get('pause_hint').text='Your expedition is paused. Save before leaving.';
  if(this.pageName&&this.pageName!=='pack')this.renderPage();
  const dialogue=this.player.campaign.dialogue;if(dialogue&&dialogue!==this.lastDialogue){this.lastDialogue=dialogue;this.noticeTime=5;}this.noticeTime=Math.max(0,(this.noticeTime||0)-dt);
  const notice=this.noticeTime>0?(dialogue.length>105?dialogue.slice(0,100)+'... [TAB]':dialogue):'';
  this.doc.get('message').text=this.player.campaign.saveNotice||notice||game.message;
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
  if(e.type==='click'&&e.element.startsWith('tab_')){this.page(e.element.slice(4));return;}
  if(e.type==='click'&&e.element.startsWith('pack_')){this.page(e.element.slice(5));return;}
  if(this.pageName&&this.pageName!=='pack'){
   if(e.type==='back'||e.type==='click'&&e.element==='field_close'){this.menu(false);return;}
   if(e.type==='click'&&e.element.startsWith('field_action_')){const a=this.actions[Number(e.element.slice(13))];if(a?.enabled)a.run();}return;
  }
  if(e.type==='click'&&e.element==='save_expedition'){this.command('save');return;}
  if(e.type==='click'&&e.element==='load_expedition'){this.command('load');return;}
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
