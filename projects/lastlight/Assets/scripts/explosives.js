import {world,physics,signals} from 'judas';
import {game} from './game.js';import {tripBones} from './combat.js';
import {add,sub,mul,length,norm,qm,axis} from './math.js';
function spawn(player,kind,view){const isRocket=kind==='rocket',asset=player.props[kind];if(!asset)return null;
 const filter={ignored:[player.entity]},sight=physics.raycast(view.position,view.direction,110,filter);
 const target=sight?sight.point:add(view.position,mul(view.direction,110)),direction=norm(sub(target,view.eye)),u=player.entity.character.up;
 // Physical projectiles begin outside the shooter's capsule. A real sweep
 // rejects a muzzle already inside cover, instead of shooting through it.
 const offset=add(mul(direction,.85),mul(u,-.12)),distance=length(offset);
 if(physics.sphereCast(view.eye,isRocket?.12:.09,norm(offset),distance,filter)){game.message='MUZZLE BLOCKED - step away from cover';return null;}
 const position=add(view.eye,offset),velocity=add(mul(direction,isRocket?32:12),player.entity.character.velocity);
 if(!isRocket){velocity.x+=u.x*3;velocity.y+=u.y*3;velocity.z+=u.z*3;}
 const e=world.spawnPrefab(asset,{position,rotation:qm(view.rotation,axis({x:0,y:1,z:0},Math.PI))},
  {velocity,angularVelocity:isRocket?{x:0,y:0,z:0}:mul(u,5),scripts:[{slot:1,state:{owner:player.entity.id,previous:position}}]});
 if(isRocket)e.gravity.setUniform({x:0,y:0,z:0});return e;
}
export function launchRocket(player,view){if(game.rockets<=0){game.message='NO ROCKETS - buy two for 250 points at the blue stall (N)';return false;}
 const e=spawn(player,'rocket',view);if(!e)return false;game.rockets--;game.shots++;game.noiseTime=1.5;game.noisePosition=view.eye;
 const fx=world.entity('21');if(fx.valid){fx.transform={position:e.transform.position};fx.burst(10);}
 player.entity.setAudio({pitch:.55});player.entity.playAudioOneShot();return true;}
export function throwGrenade(player){if(game.grenades<=0){game.message='NO GRENADES - buy two for 150 points at the blue stall (N)';return false;}
 const e=spawn(player,'grenade',player.camera.pose());if(!e)return false;game.grenades--;game.message='GRENADE OUT - 2.4 second fuse';return true;}
function visible(from,to,ignored){const delta=sub(to,from),distance=length(delta);return distance<.05||!physics.raycast(from,norm(delta),Math.max(0,distance-.08),{ignored});}
function impulse(body,center,radius,point,distance){
 if(!body?.valid||body.motionType!=='dynamic')return;
 const offset=sub(point,center),fallback=sub(body.transform.position,center);
 const direction=norm(length(offset)>.02?offset:length(fallback)>.02?fallback:mul(body.gravity.acceleration,-1));
 // Finite authored delta-speed, converted using the body's real mass. Applying
 // it at the queried surface point also gives ordinary off-centre rotation.
 const speed=Math.max(0,1-distance/radius)*6;
 body.applyImpulseAtPoint(mul(direction,body.mass*speed),point);
}
function blastBodies(projectile,center,radius,damage,owner,characters){
 const extent={x:radius,y:radius,z:radius},ignored=[projectile,...characters];
 // Broadphase supplies ordinary enabled colliders, including untagged props
 // and transient ragdoll bones. Gameplay tags do not gate physical force.
 const candidates=world.overlap(sub(center,extent),add(center,extent),{ignored});
 const seen=new Set();
 for(const body of candidates){
  if(!body?.valid||seen.has(body.id))continue;seen.add(body.id);
  const destructible=body.hasTag('destructible');
  if(body.motionType!=='dynamic'&&!destructible)continue;
  // Select this candidate's real nearest surface, not its AABB or entity
  // origin. Other candidates are ignored only for this geometry lookup;
  // the following visibility ray uses normal environment/prop occlusion.
  const others=candidates.filter(e=>e.id!==body.id);
  const hit=physics.closestPoint(center,radius,{ignored:[...ignored,...others]});
  if(!hit||hit.entityId!==body.id||!visible(center,hit.point,[...ignored,body]))continue;
  impulse(body,center,radius,hit.point,hit.distance);
  // Static authored breakable doors still take game damage, without accessing
  // dynamic-only velocity/mass APIs. Their debris becomes normal physics.
  if(destructible)signals.send(body,'range.hit',{kind:'blast',damage:Math.ceil(damage*(1-hit.distance/radius*.7)),direction:norm(sub(hit.point,center)),point:hit.point,source:owner});
 }
}
export function detonate(projectile,owner,center,radius,damage){
 const enemies=world.queryTags(['enemy']),player=world.entity('10');
 // Characters do not act as blast-proof cover for one another. Ordinary
 // solid environment/props still occlude these explicit read-only queries.
 const characters=player.valid?[player,...enemies]:enemies;
 for(const enemy of enemies){if(!enemy.valid)continue;const state=enemy.scriptState(1);if(!state||state.dead)continue;
  const visual=enemy.children.find(e=>e.valid&&e.ragdoll),ragdoll=visual?.ragdoll,parts=ragdoll?.active?tripBones.map(k=>ragdoll.body(k)).filter(e=>e?.valid):[];
  const chest=parts.length?ragdoll.body('chest'):null,up=enemy.character.up;
  const point=chest?.valid?chest.transform.position:add(enemy.transform.position,mul(up,.9)),offset=sub(point,center),distance=length(offset);
  if(distance>radius||!visible(center,point,[projectile,...characters,...parts]))continue;
  const falloff=1-distance/radius*.65,direction=norm(offset);
  signals.send(enemy,'range.hit',{kind:'blast',damage:Math.ceil(damage*falloff),direction,speed:6*(1-distance/radius),point:point,source:owner});
  game.flash=.25;
 }
 blastBodies(projectile,center,radius,damage,owner,characters);
 if(player.valid){const point=add(player.transform.position,mul(player.character.up,.9)),distance=length(sub(point,center));
  if(distance<radius&&visible(center,point,[projectile,player]))signals.send(player,'player.hurt',{damage:Math.ceil(55*(1-distance/radius))});}
 const fx=world.entity('20');if(fx.valid){fx.transform={position:center};fx.burst(65);}
 const cue=world.entity('26');if(cue.valid){cue.transform={position:center};cue.playAudioOneShot();}
 game.noiseTime=2;game.noisePosition=center;game.message='BLAST - ordinary walls stop damage';
}
