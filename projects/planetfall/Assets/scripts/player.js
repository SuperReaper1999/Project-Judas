import Interaction from './interaction.js';
import Jobs from './side-jobs.js';
import Campaign from './planetfall.js';import Rover from './rover.js';
import {input,world,signals,navigation,console,session,scenes} from 'judas';
import Camera from './camera.js';import Weapon from './weapon.js';import HUD from './hud.js';import Climb from './climb.js';import Melee from './melee.js';import Grab from './grab.js';import Vendor from './vendor.js';
import Ship from './ship.js';import {destinations} from './expedition.js';
import Inventory from './inventory.js';import Framewalk from './framewalk.js';
import {throwGrenade} from './explosives.js';import {game,spawns} from './game.js';import {add,mul,sub,dot,length,norm,tangent,qm,axis,rotate} from './math.js';
export const properties={speed:{type:'number',default:4.8},jumpSpeed:{type:'number',default:5.8},zombie:{type:'string',default:''},rocket:{type:'string',default:''},grenade:{type:'string',default:''},framewalkBoots:{type:'string',default:''},rocketsPickup:{type:'string',default:''},grenadesPickup:{type:'string',default:''},building:{type:'string',default:''},pirate:{type:'string',default:'2fc690d564ab543f28e01298c8bc6df4'}};
export default class Player {
 constructor({entity,properties}){this.entity=entity;this.props=properties;this.state={yaw:0,pitch:0,wave:0,score:0,health:100,climbing:false,view:'first',holding:null};this.enemies=[];this.toSpawn=0;this.spawnClock=0;this.spawnIndex=0;this.clip='';this.animClock=1;this.moving=false;this.grenadeCooldown=0;}
 start(){const arrival=session.get('planetfall-arrival');if(!this.state.inventory&&session.get('planetfall')?.inventory)this.state.inventory=session.get('planetfall').inventory;this.inventory=new Inventory(this);this.boots=new Framewalk(this);this.camera=new Camera(this);this.weapon=new Weapon(this);this.hud=new HUD(this);this.climb=new Climb(this);this.melee=new Melee(this);this.grab=new Grab(this);this.vendor=new Vendor(this);this.hud.start();this.ship=new Ship(this);this.campaign=new Campaign(this);this.campaign.start();this.rover=new Rover(this);this.jobs=new Jobs(this);this.interaction=new Interaction(this);if(arrival){this.ship.seat();session.delete('planetfall-arrival');}this.camera.publish();this.token=signals.subscribe('player.hurt');this.avatar=world.entity('11');this.gun=world.entity('12');console.log('Planetfall: WASD move, mouse look, Space jump/mantle, E contextual action, Tab expedition menu, Escape pause.');}
 update(dt){if(this.hud.doc.modal)return;if(this.ship.look(dt)){if(input.pressed('camera_toggle'))this.camera.third=!this.camera.third;return;}const scale=this.weapon.lookScale;
  this.state.yaw-=(input.axis('look_x')*.0021+input.axis('look_stick_x')*2.2*dt)*scale;
  this.state.pitch=Math.max(-1.4,Math.min(1.4,this.state.pitch-(input.axis('look_y')*.0021+input.axis('look_stick_y')*2.2*dt)*scale));
  if(input.pressed('camera_toggle'))this.camera.third=!this.camera.third;
 }
 onSignal(e){if(e.name!=='player.hurt'||game.phase==='dead'||game.phase==='victory')return;game.health=Math.max(0,game.health-e.payload.damage);game.hurt=.18;
  if(game.health===0){game.phase='dead';game.message='Expedition lost. Your outpost and journal remain.';this.grab.drop();this.entity.character.velocity={x:0,y:0,z:0};}}
 animate(dt){this.animClock+=dt;const m=this.entity.character,u=m.up;
  const speed=length(tangent(sub(m.velocity,m.state.supportVelocity),u));if(speed>.4)this.moving=true;else if(speed<.15)this.moving=false;
  const attack=this.melee.busy,clip=attack?this.melee.clip:this.climb.active||!m.supported?'Jump':this.moving?'Run':'Idle';
  const changedAttack=attack&&this.meleeSerial!==this.melee.serial;
  try{const a=this.avatar.animation;if(!a?.info.ready)return;
   if((this.clip!==clip||changedAttack)&&(this.animClock>=.24||changedAttack)){a.loop=!attack;a.crossFade(clip,attack ? .05 : .15);this.clip=clip;this.meleeSerial=this.melee.serial;this.animClock=0;}
   a.speed=attack?1:clip==='Run'?Math.max(.65,Math.min(1.7,speed/4.8)):1;
  }catch(error){if(!this.animationWarning){this.animationWarning=true;console.log(`Player animation fallback: ${error.message}`);}try{this.avatar.animation.play(clip);this.clip=clip;this.animClock=0;}catch(ignored){}}
 }
 fixedUpdate(dt){if(!this.hud.doc.modal){this.interaction.tick();this.campaign.tick(dt);this.jobs.tick(dt);}game.flash=Math.max(0,game.flash-dt);game.hurt=Math.max(0,game.hurt-dt);game.noiseTime=Math.max(0,game.noiseTime-dt);game.footstepTime=Math.max(0,game.footstepTime-dt);this.grenadeCooldown=Math.max(0,this.grenadeCooldown-dt);
  if(game.phase==='dead'||game.phase==='victory')return;
  if(this.hud.doc.modal)return;if(this.rover.tick()){this.state.view=this.camera.third?'third':'first';return;}const shipUsed=this.ship.tick(dt);if(this.ship.piloting){this.state.view=this.camera.third?'third':'first';return;}this.weapon.tick(dt);this.vendor.tick();if(this.hud.doc.modal)return;this.inventory.tick();if(!this.ship.inside)this.boots.tick(dt);
  const m=this.entity.character,t=this.entity.transform,u=m.up;
  const heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));
  const f=norm(tangent(rotate(heading,{x:0,y:0,z:-1}),u)),r=norm(tangent(rotate(heading,{x:1,y:0,z:0}),u));
  if(!shipUsed&&(input.pressed('climb')||input.pressed('jump')&&m.supported)&&!this.melee.busy&&!this.grab.held&&!this.boots.state.transitioning)this.climb.begin(f);
  if(!this.climb.step(dt)){
   let dir=add(mul(f,input.axis('move_y')),mul(r,input.axis('move_x')));if(length(dir)>1)dir=norm(dir);
   const pace=this.grab.held?3.3:this.weapon.aimed?3.1:input.held('sprint')?7:this.props.speed;
   const desired=mul(dir,pace),state=m.state;let velocity=m.velocity;
   const carry=state.supported?state.supportVelocity:{x:0,y:0,z:0},own=tangent(sub(velocity,carry),u),change=sub(desired,own);
   const next=add(own,mul(norm(change),Math.min(length(change),(state.supported?28:9)*dt)));
   velocity=add(add(carry,next),mul(u,dot(sub(velocity,carry),u)));
   if(input.pressed('jump')&&!this.climb.active){if(state.supported){velocity=add(velocity,mul(u,this.props.jumpSpeed));this.boots.launch();}else this.boots.airborneJump();}m.velocity=velocity;
   if(state.supported&&length(desired)>1.2){game.footstepTime=.25;game.footstepPosition=t.position;}
  }
  this.grab.tick(dt);if(!input.held('fire'))this.fireBlocked=false;
  if(!this.grab.held){this.melee.tick(dt);if(input.held('fire')&&!this.fireBlocked&&!this.melee.busy&&!this.climb.active)this.weapon.fire();
   if(input.pressed('grenade')&&!this.melee.busy&&!this.climb.active&&this.grenadeCooldown<=0&&throwGrenade(this))this.grenadeCooldown=1;}
  this.animate(dt);this.state.wave=game.wave;this.state.score=game.score;this.state.health=game.health;this.state.climbing=!!this.climb.active;this.state.view=this.camera.third?'third':'first';
 }
 presentationUpdate(){if(this.ship.piloting||this.rover?.active){const view=this.camera.publish(true);this.avatar.renderVisible=false;this.gun.renderVisible=false;this.weapon.launcher.renderVisible=false;for(const id of ['13','14'])world.entity(id).renderVisible=false;return;}const t=this.entity.presentedTransform,view=this.camera.publish(true);
  this.avatar.renderVisible=this.camera.third;
  this.avatar.transform={position:t.position,rotation:qm(qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:0,y:1,z:0},Math.PI))};
  this.weapon.present(view);if(this.grab.held){this.gun.renderVisible=false;this.weapon.launcher.renderVisible=false;}this.melee.present(view);
 }
 restore(){const vehicle=this.state.vehicle;this.start();if(vehicle==='ship')this.ship.seat();if(vehicle==='rover'){this.rover.active=true;this.entity.character.enabled=false;}}
 uiUpdate(dt){this.hud.update(dt);}onUI(e){this.hud.event(e);}
 destroy(){try{scenes.removeInterest('planetfall-player');}catch(ignored){}this.ship?.destroy();this.grab?.destroy();this.boots?.destroy();if(this.token)signals.unsubscribe(this.token);input.pointerCapture=false;world.clearView();}
}
