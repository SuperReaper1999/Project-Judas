import {world,input,ui,scenes,saves,weather} from 'judas';
import {add,sub,mul,dot,length,norm,cross,tangent,look} from './math.js';
import {recipes,names,tourDurations} from './profiles.js';
export const properties={planet:{type:'boolean',default:false}};
const ASSETS={traveller:'c13e36165b9141c9b38b487eecbce9bb',probe:'02d2e101aacf1f967ef2e808721350a2'};
const UP={x:0,y:1,z:0};
export default class {
 constructor({properties}){this.props=properties;this.state={mode:'landing',direction:UP,altitude:1.7,yaw:0,pitch:.06,actor:null,balls:[],tour:false,tourTime:0,tourIndex:0,selected:0};this.pending=[];}
 start(){this.bind();}
 restore(){this.bind();}
 bind(){this.planet=this.props.planet?world.entity('1'):null;this.field=world.entity('20').weather;this.hud=ui.get('weather_lab');this.hud.modal=false;this.hud.get('pause').visible=false;ui.debugOverlayVisible=false;input.pointerCapture=true;this.problem='';this.pending=[];}
 surface(direction){if(!this.props.planet)return {position:{x:0,y:0,z:0},distance:0};const s=this.planet.planetState;return this.planet.planetSample(add(s.center,mul(direction,s.radius)));}
 get eye(){if(this.state.actor){const a=world.entity(this.state.actor);if(a?.valid)return add(world.toAbsolute(a.presentedTransform.position),mul(this.state.direction,.7));}return add(this.surface(this.state.direction).position,mul(this.state.direction,this.state.altitude));}
 leaveGround(){if(this.state.actor){const a=world.entity(this.state.actor);if(a?.valid&&this.props.planet){this.state.direction=norm(sub(world.toAbsolute(a.transform.position),this.planet.planetState.center));this.state.altitude=Math.max(.1,this.planet.planetSample(this.eye).distance);}if(a?.valid)a.destroy();this.state.actor=null;}for(const id of this.state.balls){const b=world.entity(id);if(b?.valid)b.destroy();}this.state.balls=[];scenes.removeInterest('weather-traveller');if(this.planet?.valid)this.planet.clearPlanetCollision();this.state.mode='flight';}
 uiUpdate(){if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}}
 onUI(e){if(e.type==='click'&&e.element==='resume'){this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;}if(e.type==='click'&&e.element==='reload')scenes.reload();}
 update(dt){if(this.hud.modal)return;
  this.state.yaw+=input.axis('look_x')*.002;this.state.pitch=Math.max(-1.35,Math.min(1.35,this.state.pitch-input.axis('look_y')*.002));
  for(let i=1;i<=8;i++)if(input.pressed('weather_'+i)){this.pending.push(i-1);this.state.tour=false;}
  if(input.pressed('watch')){this.state.tour=!this.state.tour;this.state.tourTime=0;this.state.tourIndex=0;if(this.state.tour)this.pending.push(0);}
  if(input.pressed('quality'))weather.quality=weather.quality===2?1:2;
  if(input.pressed('visual_off'))weather.quality=weather.quality===0?1:0;
  if(input.pressed('switch_scene'))scenes.load(this.props.planet?'Scenes/flat.judas':'Scenes/planet.judas');
  if(input.pressed('orbit')){this.leaveGround();this.state.mode='ascending';this.state.tour=false;}
  if(input.pressed('land')){this.leaveGround();this.state.mode='descending';this.state.tour=false;}
  if(input.pressed('edge')&&this.props.planet){this.leaveGround();this.state.direction=norm({x:1,y:1,z:0});this.state.altitude=1200;this.state.mode='descending';this.state.tour=false;}
  if(input.pressed('reset'))scenes.reload();if(input.pressed('save'))this.request=saves.save(this.props.planet?'weather-planet':'weather-flat');if(input.pressed('load'))this.request=saves.load(this.props.planet?'weather-planet':'weather-flat');
  if(input.pressed('drop')&&this.state.actor){const pose=world.entity(this.state.actor).transform,basis=this.basis();const ball=world.spawnPrefab(ASSETS.probe,{position:add(pose.position,add(mul(this.state.direction,1),mul(basis.forward,2))),rotation:pose.rotation});if(ball){ball.applyImpulse(add(mul(basis.forward,8),mul(this.state.direction,4)));this.state.balls.push(ball.id);}}
  if(this.state.mode==='ascending')this.state.altitude=Math.min(this.props.planet?4000000:1400,this.state.altitude+Math.max(70,this.state.altitude*1.25)*dt);
  if(this.state.mode==='descending'){this.state.altitude=Math.max(1.7,this.state.altitude*Math.exp(-.65*dt));if(this.state.altitude<=1.701)this.state.mode='landing';}
  if(this.props.planet&&(this.state.mode==='flight'||this.state.mode==='ascending')){const side=this.basis().right;this.state.direction=norm(add(this.state.direction,mul(side,dt*.06*(Number(input.held('move_right'))-Number(input.held('move_left'))))));}
 }
 basis(){const up=this.state.direction,base=norm(tangent(Math.abs(up.z)<.9?{x:0,y:0,z:-1}:{x:1,y:0,z:0},up)),right=cross(base,up);return {forward:add(mul(base,Math.cos(this.state.yaw)),mul(right,Math.sin(this.state.yaw))),right:add(mul(right,Math.cos(this.state.yaw)),mul(base,-Math.sin(this.state.yaw)))};}
 select(index){this.field.transition(recipes[index],3);this.state.selected=index;}
 fixedUpdate(dt){if(this.hud.modal)return;
  for(const index of this.pending)this.select(index);this.pending=[];
  if(this.state.tour){this.state.tourTime+=dt;let start=0,index=0;for(let i=0;i<tourDurations.length;i++){if(this.state.tourTime>=start)index=i;start+=tourDurations[i];}if(this.state.tourTime>start){this.state.tourTime=0;index=0;}if(index!==this.state.tourIndex){this.state.tourIndex=index;this.select(index);if(this.props.planet&&index===1){this.leaveGround();this.state.mode='ascending';}if(this.props.planet&&index===2){this.leaveGround();this.state.mode='descending';}}}
  if(this.state.mode==='landing'){
   const absolute=this.props.planet?add(this.surface(this.state.direction).position,mul(this.state.direction,1.0)):{x:0,y:1.05,z:12};
   try{if(this.props.planet&&length(sub(absolute,world.origin))>1000){if(!world.originState.pending)world.requestOrigin(absolute);return;}
    if(this.props.planet&&!this.planet.preparePlanetCollision(absolute)){this.problem='Preparing physical terrain...';return;}
    const actor=world.spawnPrefab(ASSETS.traveller,{position:world.toLocal(absolute),rotation:look(this.basis().forward,this.state.direction)});if(!actor)throw Error('traveller prefab unavailable');this.state.actor=actor.id;this.state.mode='ground';this.problem='';if(this.props.planet)scenes.setInterest('weather-traveller',actor.transform.position,{load:60,retain:100,priority:5});
   }catch(error){this.problem=String(error);}return;
  }
  if(this.state.mode!=='ground'){if(this.props.planet)try{if(length(world.toLocal(this.eye))>1500&&!world.originState.pending)world.requestOrigin(this.eye);this.problem='';}catch(error){this.problem=String(error);}return;}
  const actor=world.entity(this.state.actor);if(!actor?.valid)return;const motor=actor.character;
  if(this.props.planet){const absolute=world.toAbsolute(actor.transform.position);this.state.direction=norm(sub(absolute,this.planet.planetState.center));scenes.setInterest('weather-traveller',actor.transform.position,{load:60,retain:100,priority:5});try{if(!this.planet.preparePlanetCollision(absolute)){motor.velocity={x:0,y:0,z:0};return;}if(length(actor.transform.position)>1500&&!world.originState.pending)world.requestOrigin(absolute);this.problem='';}catch(error){this.problem=String(error);motor.velocity={x:0,y:0,z:0};return;}}
  const b=this.basis(),up=this.state.direction,desired=this.state.tour?{x:0,y:0,z:0}:mul(norm(add(mul(b.right,Number(input.held('move_right'))-Number(input.held('move_left'))),mul(b.forward,Number(input.held('move_forward'))-Number(input.held('move_back'))))),5);let velocity=motor.velocity;
  if(motor.supported){velocity=add(desired,mul(up,dot(velocity,up)));if(input.pressed('jump'))velocity=add(velocity,mul(up,5));}else velocity=add(velocity,mul(sub(desired,tangent(velocity,up)),Math.min(1,dt*3)));motor.velocity=velocity;
 }
 presentationUpdate(){let eye=this.eye;const b=this.basis(),up=this.state.direction;let forward=add(mul(b.forward,Math.cos(this.state.pitch)),mul(up,Math.sin(this.state.pitch)));
  if(!this.props.planet&&this.state.tour){const t=this.state.tourTime*.045;eye={x:22*Math.sin(t),y:6.2,z:14+4*Math.cos(t)};forward=norm(sub({x:0,y:7,z:-24},eye));}
  if(this.state.mode!=='ground'){const blend=Math.max(0,Math.min(1,(Math.log(Math.max(1,this.state.altitude))-Math.log(10))/(Math.log(500)-Math.log(10))));forward=norm(add(mul(forward,1-blend),mul(norm(add(mul(up,-1),mul(b.forward,.12))),blend)));}
  world.setView({position:world.toLocal(eye),rotation:look(forward,Math.abs(dot(forward,up))>.99?b.forward:up)},70,{near:this.state.mode==='ground'?.05:Math.max(.05,this.state.altitude*.002),far:this.props.planet?Math.max(200000,this.state.altitude+15000000):5000});
  const sample=weather.sample(world.toLocal(eye)),state=this.field.state;const mode=this.props.planet?'EARTH / 6371 KM':'FLAT / MEADOW';let text=`${mode}    |    ${names[this.state.selected].toUpperCase()}${this.state.tour?'  /  WATCH TOUR':''}    |    ${weather.quality===0?'VISUALS OFF':weather.quality===1?'QUARTER RES':'HALF RES'}\n`;
  if(sample)text+=`Local wind ${length(sample.wind).toFixed(1)} m/s   |   Rain ${(sample.rain*100).toFixed(0)}%   Snow ${(sample.snow*100).toFixed(0)}%   |   Altitude ${sample.altitude.toFixed(0)} m\n`;
  text+='WASD + mouse / Space jump / E probe  |  Walk below pavilion roof to test shelter\nF1 ascend / F2 land / F3 planet face  |  F6 save / F7 load / R reload';
  if(this.request){const r=saves.status(this.request);text+=`\nSave: ${r?.state||'pending'} ${r?.error||''}`;}if(this.problem)text+='\n'+this.problem;this.hud.get('status').text=text;
 }
 destroy(){if(this.planet?.valid)this.planet.clearPlanetCollision();scenes.removeInterest('weather-traveller');input.pointerCapture=false;world.clearView();}
}
