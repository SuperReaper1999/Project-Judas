import {world,input,ui,players,scenes,saves} from 'judas';
import {CONTENT as C} from './content.js';
import {NAMES,KEYS,bindings,viewport} from './controls.js';
import {v,sub,mul,len,norm,camera} from './math.js';
const GOALS=[v(-8,0,-8),v(8,0,8),v(8,0,-8),v(-8,0,8)];
export default class CoreScramble {
 constructor(){this.state={time:90,scores:[0,0,0,0],split:false,count:2,finished:false,bots:[null,null,null,null],joined:[true,true,false,false],cooldowns:[0,0,0,0]};this.roster=[];this.seenPads=new Set();this.cooldowns=this.state.cooldowns;this.touchStarts=new Map();this.feedback='PUSH THE WHITE CORES INTO YOUR COLOUR';this.spawnClock=0;}
 start(){this.loadHUD();ui.debugOverlayVisible=false;input.pointerCapture=false;for(let i=0;i<this.state.count;i++)this.join(i);for(let i=0;i<5;i++)this.spawnCore(i);this.layout();}
 // Runtime player/device handles intentionally are not persistent identifiers.
 // Rebuild local seats and input ownership from project policy after a reload.
 restore(){this.loadHUD();ui.debugOverlayVisible=false;input.pointerCapture=false;this.cooldowns=this.state.cooldowns;for(let i=0;i<4;i++)if(this.state.joined[i])this.join(i,saves.resolve(this.state.bots[i]));this.layout();if(this.state.finished)this.finish();}

 loadHUD(){const portrait=world.viewport.width<world.viewport.height;if(!this.hud){this.hud=ui.get('scramble');this.hudPortrait=false;}if(this.hud&&portrait===this.hudPortrait)return;const modal=this.hud?.modal||false;if(this.hud)this.hud.unload();this.hud=ui.load(C[portrait?'hudPortrait':'hud'],'scramble');this.hudPortrait=portrait;if(this.state.finished)this.finish();else this.globalMenu(modal);}

 join(slot,restored=null){if(this.roster[slot])return;const p=players.create(NAMES[slot],bindings(slot));p.entity=restored?.valid?restored:world.spawnPrefab(C['bot'+slot],{position:v(GOALS[slot].x*.7,1.1,GOALS[slot].z*.7)});this.state.bots[slot]=saves.reference(p.entity);this.state.joined[slot]=true;const doc=ui.load(C.local,'seat-'+p.id);doc.player=p;doc.get('label').text='P'+(slot+1)+' / '+NAMES[slot];this.roster[slot]={p,doc,slot,device:'KEYBOARD',menu:false};this.assignDefault(slot);this.layout();}
 leave(slot){const seat=this.roster[slot];if(!seat)return;if(seat.p.entity?.valid)seat.p.entity.destroy();seat.p.leave();this.roster[slot]=null;this.state.bots[slot]=null;this.state.joined[slot]=false;this.feedback=NAMES[slot]+' LEFT / F2–F4 OR BUTTONS TO REJOIN';this.layout();}
 assignDefault(slot){const seat=this.roster[slot];if(!seat)return;try{seat.p.assign('keyboard',{controls:KEYS[slot].map(k=>'key:'+k)});if(slot===0)seat.p.assign('mouse');seat.device='KEYBOARD';}catch(e){this.feedback=e.message;}}
 changeDevice(slot){const seat=this.roster[slot];if(!seat)return;const pads=players.devices.filter(d=>d.connected&&d.kind==='gamepad'),choices=['KEYBOARD',...pads.map(d=>d.id)];let start=choices.indexOf(seat.device);for(let k=1;k<=choices.length;k++){const device=choices[(start+k)%choices.length];if(device!=='KEYBOARD'&&players.list.some(p=>p.id!==seat.p.id&&p.devices.includes(device)))continue;
  for(const old of seat.p.devices)seat.p.unassign(old);
  if(device==='KEYBOARD')this.assignDefault(slot);else{try{seat.p.assign(device);seat.device=device;}catch(e){this.feedback=e.message;this.assignDefault(slot);}}return;}}
 layout(){const seats=this.roster.filter(Boolean),touch=new Set(seats.filter(s=>s.device==='KEYBOARD').map(s=>s.p.id));for(const s of seats)if(s.p.devices.includes('touch'))s.p.unassign('touch');for(let n=0;n<seats.length;n++){const s=seats[n];s.p.viewport=viewport(n,Math.max(2,seats.length),world.viewport.width<world.viewport.height);const bottom=!this.state.split&&seats.length===2&&n===1&&world.viewport.width<world.viewport.height;for(const [id,x,y]of[['strip',12,12],['label',25,17],['score',25,46],['options',-20,23]])s.doc.get(id).setLayout({offset:{x,y:y+(bottom?396:0)}});for(const [id,x]of[['move_hint',24],['shove',-24]])s.doc.get(id).setLayout({offset:{x,y:bottom?-135:-64}});s.doc.get('cooldown').visible=!bottom;if(touch.has(s.p.id))try{s.p.assign('touch',{region:s.p.viewport});}catch(e){this.feedback=e.message;}if(!this.state.split)s.p.clearView();}}

 resizeRoster(count){for(let i=count;i<4;i++)this.leave(i);this.state.count=count;for(let i=0;i<count;i++)this.join(i);this.layout();}
 menu(seat,on){seat.menu=on;seat.doc.modal=on;seat.doc.get('panel').visible=on;seat.doc.get('options').visible=!on;seat.doc.get('shove').visible=!on;}
 globalMenu(on){this.hud.modal=on;this.hud.get('shade').visible=on;this.hud.get('resume').visible=!this.state.finished;}
 onUI(e){if(e.document==='scramble'){if(e.type==='back'){if(!this.state.finished)this.globalMenu(false);return;}if(e.type!=='click')return;if(e.element==='lobby')this.globalMenu(true);else if(e.element.startsWith('input')){this.changeDevice(Number(e.element.slice(5)));this.layout();}else if(e.element==='resume')this.globalMenu(false);else if(e.element==='restart')scenes.reload();else if(e.element==='quit')ui.quit();else if(e.element==='split'){this.state.split=!this.state.split;this.layout();}else if(['two','three','four'].includes(e.element))this.resizeRoster({two:2,three:3,four:4}[e.element]);return;}
  const seat=this.roster.find(s=>s&&s.doc.player?.id===e.playerId);if(!seat)return;
  if(e.type==='back'||(e.type==='click'&&e.element==='resume'))this.menu(seat,false);
  if(e.type==='click'&&e.element==='options')this.menu(seat,true);
  if(e.type==='click'&&e.element==='device'){this.changeDevice(seat.slot);this.layout();}
  if(e.type==='click'&&e.element==='shove')seat.queuedShove=true;
  if(e.type==='click'&&e.element==='leave')this.leave(seat.slot);
 }
 uiUpdate(){this.loadHUD();if(input.pressed('pause')&&!this.hud.modal)this.pendingPause=true;if(input.pressed('split')){this.state.split=!this.state.split;this.layout();}if(input.pressed('restart'))scenes.reload();for(const [name,n]of[['two',2],['three',3],['four',4]])if(input.pressed(name))this.resizeRoster(n);
  for(const seat of this.roster)if(seat){if(seat.p.input.pressed('pause')&&!seat.menu)seat.pendingMenu=true;const disconnected=seat.p.devices.some(id=>!players.devices.some(d=>d.id===id&&d.connected));seat.doc.get('score').text=this.state.scores[seat.slot]+' CORES / '+(disconnected?'DISCONNECTED':seat.device==='KEYBOARD'?'KEYS + TOUCH':'GAMEPAD');seat.doc.get('cooldown').text=this.cooldowns[seat.slot]>0?'SHOVE '+this.cooldowns[seat.slot].toFixed(1)+'s':'SHOVE READY / TAP RIGHT';}
  // Connection is a join opportunity, not player identity. A replacement never
  // inherits the old device token or silently controls an existing seat.
  const connectedPads=players.devices.filter(d=>d.connected&&d.kind==='gamepad'),newPads=connectedPads.filter(d=>!this.seenPads.has(d.id));this.seenPads=new Set(connectedPads.map(d=>d.id));
  const claimed=new Set(players.list.flatMap(p=>p.devices));for(const device of newPads)if(!claimed.has(device.id)){
   let seat=this.roster.find(s=>s&&s.device==='KEYBOARD');if(!seat){const slot=[0,1,2,3].find(i=>!this.roster[i]);if(slot!==undefined){this.state.count=Math.max(2,slot+1);this.join(slot);seat=this.roster[slot];}}
   if(seat){for(const old of seat.p.devices)seat.p.unassign(old);seat.p.assign(device.id);seat.device=device.id;this.feedback=NAMES[seat.slot]+' GAMEPAD CONNECTED';claimed.add(device.id);}
  }
  this.hud.get('timer').text=Math.max(0,Math.ceil(this.state.time))+' SEC / '+(this.state.split?'SPLIT VIEWS':'SHARED ARENA');this.hud.get('notice').text=this.feedback;
 }
 update(){for(const s of this.roster)if(s?.pendingMenu){s.pendingMenu=false;this.menu(s,true);}if(this.pendingPause){this.pendingPause=false;this.globalMenu(true);}}
 spawnCore(index=0){const angle=index*2.4+this.spawnClock;return world.spawnPrefab(C.core,{position:v(Math.cos(angle)*2,1.4,Math.sin(angle)*2)});}
 shove(seat){const bot=seat.p.entity;if(!bot?.valid||this.cooldowns[seat.slot]>0)return;this.cooldowns[seat.slot]=1.2;world.entity('960').playAudioOneShot();for(const target of [...world.queryTags(['core']),...world.queryTags(['cargo']),...world.queryTags(['bot'])])if(target.id!==bot.id){const d=sub(target.transform.position,bot.transform.position);if(len(d)<3.1){const direction=norm(v(d.x,.28,d.z));target.applyImpulse(mul(direction,target.mass*(target.hasTag('core')?7:3)));}}}
 fixedUpdate(dt){if(this.state.finished)return;this.state.time-=dt;this.spawnClock+=dt;for(let i=0;i<4;i++)this.cooldowns[i]=Math.max(0,this.cooldowns[i]-dt);
  const liveTouches=new Set();for(const seat of this.roster)if(seat&&seat.p.entity?.valid){const bot=seat.p.entity,controls=seat.p.input;const position=bot.transform.position;if(position.y< -7||Math.abs(position.x)>16||Math.abs(position.z)>16){bot.destroy();seat.p.entity=world.spawnPrefab(C['bot'+seat.slot],{position:v(GOALS[seat.slot].x*.7,1.2,GOALS[seat.slot].z*.7)});this.state.bots[seat.slot]=saves.reference(seat.p.entity);this.state.scores[seat.slot]=Math.max(0,this.state.scores[seat.slot]-1);this.feedback=NAMES[seat.slot]+' RECOVERED / -1 CORE';continue;}let x=controls.axis('x'),z=controls.axis('z'),fire=controls.pressed('shove')||(!seat.menu&&seat.queuedShove);seat.queuedShove=false;const region=seat.p.viewport;
   for(const touch of controls.touches){const touchKey=seat.p.id+"/"+touch.id;liveTouches.add(touchKey);if(!this.touchStarts.has(touchKey))this.touchStarts.set(touchKey,{x:touch.x,y:touch.y,used:false});const start=this.touchStarts.get(touchKey),localX=(start.x-region.x)/region.width,localY=(start.y-region.y)/region.height;if(localY<.30||localY>.87)continue;
    if(localX>.57){if(!start.used){fire=true;start.used=true;}}else{const dx=(touch.x-start.x)/region.width,dz=(touch.y-start.y)/region.height;x=Math.max(-1,Math.min(1,dx*8));z=Math.max(-1,Math.min(1,dz*8));}}
   const magnitude=Math.hypot(x,z);if(magnitude>1){x/=magnitude;z/=magnitude;}const current=bot.velocity;const force=v((x*7-current.x)*bot.mass*9,0,(z*7-current.z)*bot.mass*9),f=len(force);bot.applyForce(f>500?mul(force,500/f):force);
   if(controls.pressed('jump')&&bot.transform.position.y<1.1)bot.applyImpulse(v(0,bot.mass*5,0));if(fire)this.shove(seat);
  }
  for(const id of this.touchStarts.keys())if(!liveTouches.has(id))this.touchStarts.delete(id);
  for(const core of world.queryTags(['core'])){const p=core.transform.position;if(Math.abs(p.x)>15||Math.abs(p.z)>15||p.y< -5){core.destroy();this.spawnCore();continue;}for(const seat of this.roster)if(seat&&p.y<1.5&&Math.hypot(p.x-GOALS[seat.slot].x,p.z-GOALS[seat.slot].z)<1.75){this.state.scores[seat.slot]++;core.destroy();this.spawnCore();world.entity('961').playAudioOneShot();this.feedback=NAMES[seat.slot]+' SCORES! / REAL BODY ENTERED GOAL';break;}}
  if(this.state.time<=0)this.finish();
 }
 finish(){this.state.finished=true;const active=this.roster.filter(Boolean),best=Math.max(0,...active.map(s=>this.state.scores[s.slot])),winners=active.filter(s=>this.state.scores[s.slot]===best);this.hud.get('menu_title').text=winners.length===1?NAMES[winners[0].slot]+' WINS / '+best+' CORES':'DRAW / '+best+' CORES';this.globalMenu(true);}
 presentationUpdate(){const portrait=world.viewport.width<world.viewport.height;if(portrait!==this.portrait){this.portrait=portrait;this.layout();}const aspect=world.viewport.width/Math.max(1,world.viewport.height),h=Math.max(27,24/aspect);world.setView(camera(v(0,h,h*.70),v()),50,{near:.1,far:180});for(const seat of this.roster)if(seat){if(!this.state.split){seat.p.clearView();continue;}const p=seat.p.entity?.presentedTransform.position||v(),r=seat.p.viewport,a=aspect*r.width/r.height,height=Math.max(15,12/a);seat.p.setView(camera(v(p.x,p.y+height,p.z+height*.65),p),55,{near:.1,far:160});}}
}
