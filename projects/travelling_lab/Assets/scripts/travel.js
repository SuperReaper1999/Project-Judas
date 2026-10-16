import {world,input,ui,scenes,saves,physics} from 'judas';
import {add,sub,mul,dot,length,norm,rotate,cross,tangent,look} from './math.js';
const ROVER='90909090909090909090909000000002',EXPLORER='90909090909090909090909000000003';
export default class {
 constructor(){this.state={ready:false,rover:null,actor:null,driving:false,watch:false,time:0,distance:0,shifts:0,yaw:0,pitch:.2};this.problem='';}
 start(){this.bind();}
 restore(){this.bind();}
 bind(){this.planet=world.entity('1');this.hud=ui.get('travel');this.hud.modal=false;this.hud.get('pause').visible=false;ui.debugOverlayVisible=false;input.pointerCapture=true;this.camera=null;this.request=null;}
 onOriginShift(event){if(this.camera)this.camera=sub(this.camera,event.delta);this.state.shifts++;}
 uiUpdate(){if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}}
 update(){if(this.hud.modal)return;this.state.yaw+=input.axis('look_x')*.002;this.state.pitch=Math.max(-.7,Math.min(1.1,this.state.pitch+input.axis('look_y')*.002));
  if(input.pressed('reload'))scenes.reload();if(input.pressed('save'))this.request=saves.save('expedition');if(input.pressed('load'))this.request=saves.load('expedition');}
 board(){const actor=world.entity(this.state.actor),rover=world.entity(this.state.rover);if(!actor?.valid||!rover?.valid)return;
  if(!this.state.driving){if(length(sub(actor.transform.position,rover.transform.position))>6){this.problem='Move within 6 metres of the rover to board.';return;}actor.character.enabled=false;actor.renderVisible=false;this.state.driving=true;}
  else {if(length(rover.velocity)>1.5){this.problem='Brake to a walking speed before leaving.';return;}const t=rover.transform,up=rotate(t.rotation,{x:0,y:1,z:0}),p=add(t.position,add(rotate(t.rotation,{x:2.4,y:0,z:0}),mul(up,1.3)));
   actor.transform={position:p,rotation:t.rotation};actor.character.velocity=rover.pointVelocity(p);actor.character.enabled=true;actor.renderVisible=true;this.state.driving=false;this.state.watch=false;}
  this.problem='';this.state.yaw=0;this.camera=null;
 }
 fixedUpdate(dt){
  if(!this.state.ready){const sample=this.planet.planetSample({x:0,y:6371000,z:0});if(!this.planet.preparePlanetCollision(sample.position))return;const up=sample.normal,q=look({x:0,y:0,z:-1},up);
   const rover=world.spawnPrefab(ROVER,{position:world.toLocal(add(sample.position,mul(up,1))),rotation:q});const actor=world.spawnPrefab(EXPLORER,{position:world.toLocal(add(sample.position,{x:3,y:1.4,z:0})),rotation:q});this.state.rover=rover.id;this.state.actor=actor.id;this.state.ready=true;
  }
  this.state.time+=dt;const rover=world.entity(this.state.rover),actor=world.entity(this.state.actor);if(!rover?.valid||!actor?.valid)return;
  if(input.pressed('watch')){this.state.watch=!this.state.watch;if(this.state.watch&&!this.state.driving)this.board();}
  if(input.pressed('use'))this.board();
  const focus=this.state.driving?rover:actor,t=focus.transform,absolute=world.toAbsolute(t.position),surface=this.planet.planetSample(absolute);
  const ready=this.planet.preparePlanetCollision(surface.position);scenes.setInterest('traveller',t.position,{load:80,retain:150});
  // Scripts request policy; native fixed-boundary translation preserves the
  // whole assembly. Busy streaming transactions defer rather than partly shift.
  if(length(t.position)>256&&!world.originState.pending){try{world.requestOrigin(absolute);}catch(error){this.problem=String(error);}}
  let throttle=Number(input.held('forward'))-Number(input.held('backward')),steering=(Number(input.held('left'))-Number(input.held('right')))*.36,brake=input.held('brake')?1:0;
  // Watch steering is ordinary game policy. Throttle, steering and brakes
  // act through real mechanical inputs; the chassis is never repositioned.
  if(this.state.watch){const speed=dot(rover.velocity,rotate(rover.transform.rotation,{x:0,y:0,z:-1}));throttle=Math.max(-.15,Math.min(.8,(15-speed)*.13));const up=surface.normal,forward=norm(tangent(rotate(rover.transform.rotation,{x:0,y:0,z:-1}),up)),course=norm(tangent({x:-absolute.x*.035,y:0,z:-1},up)),heading=Math.atan2(dot(cross(forward,course),up),dot(forward,course));steering=Math.max(-.3,Math.min(.3,heading*.9));brake=0;}
  if(!ready){throttle=0;brake=1;}
  if(!this.state.driving&&input.pressed('interact')){const ray=world.viewRay,hit=ray&&physics.raycast(ray.origin,ray.direction,5,{ignored:[actor,rover]});if(hit&&hit.entity.hasTag('Cargo'))hit.entity.applyImpulseAtPoint(mul(ray.direction,35),hit.point);}
  const drive=this.state.driving;rover.powertrain.set({sources:[{name:'engine',input:drive?throttle:0}],links:[{name:'clutch',engagement:1}],brakes:[{name:'left',engagement:drive?brake:1},{name:'right',engagement:drive?brake:1}]});
  const wheels=rover.children.filter(e=>e.wheel);for(let i=0;i<wheels.length;i++)wheels[i].wheel.set({driveTorque:0,brakeTorque:i<2?(drive?brake:1)*750:0,steeringAngle:drive&&i<2?steering:0});
  if(drive){actor.transform={position:add(rover.transform.position,rotate(rover.transform.rotation,{x:0,y:1,z:0})),rotation:rover.transform.rotation};this.state.distance+=length(rover.velocity)*dt;}
  else {const up=actor.character.up,base=norm(tangent({x:0,y:0,z:-1},up)),right=cross(base,up),forward=add(mul(base,Math.cos(this.state.yaw)),mul(right,Math.sin(this.state.yaw))),side=cross(forward,up),move=add(mul(forward,throttle),mul(side,Number(input.held('right'))-Number(input.held('left')))),v=add(mul(norm(move),length(move)>0?4:0),mul(up,dot(actor.character.velocity,up)));if(input.pressed('brake')&&actor.character.supported)actor.character.velocity=add(v,mul(up,5));else actor.character.velocity=v;}
  for(const bot of world.queryTags(['Courier'])){if(!bot.navigation||!bot.character)continue;const near=length(sub(bot.transform.position,t.position))<70;
   if(!near){bot.navigation.clear();bot.navigation.stopped=false;bot.character.velocity={x:0,y:0,z:0};continue;}
   if(input.pressed('navigate'))bot.navigation.setDestination(add(bot.transform.position,rotate(bot.transform.rotation,{x:12,y:-.9,z:0})));
   const up=bot.character.up,steering=bot.navigation.state.steering;bot.character.velocity=add(tangent(steering,up),mul(up,dot(bot.character.velocity,up)));
  }
 }
 presentationUpdate(dt){if(!this.state.ready)return;const focus=world.entity(this.state.driving?this.state.rover:this.state.actor);if(!focus?.valid)return;const t=focus.presentedTransform,up=norm(mul(physicsGravity(focus),-1));const basisUp=length(up)>.5?up:rotate(t.rotation,{x:0,y:1,z:0});let forward;
  if(this.state.driving){const base=rotate(t.rotation,{x:0,y:0,z:-1}),right=cross(base,basisUp);forward=norm(add(mul(base,Math.cos(this.state.yaw)),mul(right,Math.sin(this.state.yaw))));}else{const base=norm(tangent({x:0,y:0,z:-1},basisUp)),right=cross(base,basisUp);forward=norm(add(mul(base,Math.cos(this.state.yaw)),mul(right,Math.sin(this.state.yaw))));}
  let eye=this.state.driving?add(t.position,add(mul(forward,-7),mul(basisUp,3.8+this.state.pitch*3))):add(t.position,mul(basisUp,.65));const target=this.state.driving?add(t.position,mul(basisUp,.8)):add(eye,add(mul(forward,Math.cos(this.state.pitch)),mul(basisUp,-Math.sin(this.state.pitch))));
  if(this.state.driving){const ray=sub(eye,target),distance=length(ray),hit=physics.sphereCast(target,.2,norm(ray),distance,{ignored:[world.entity(this.state.rover),world.entity(this.state.actor)]});if(hit)eye=add(target,mul(norm(ray),Math.max(.5,hit.distance-.1)));}
  this.camera=this.camera?add(this.camera,mul(sub(eye,this.camera),1-Math.exp(-9*dt))):eye;world.setView({position:this.camera,rotation:look(sub(target,this.camera),basisUp)},70,{near:.05,far:60000});
  const rover=world.entity(this.state.rover),state=rover.powertrain.state,wheels=rover.children.filter(e=>e.wheel),contacts=wheels.filter(e=>e.wheel.state.contact).length,regions=scenes.regions;let text=`${this.state.driving?'DRIVING — E to leave after braking':'ON FOOT — E near rover to board'} | ${this.state.watch?'WATCH DRIVE — T to stop':''}\nSpeed ${length(rover.velocity).toFixed(1)} m/s | shaft ${state.shafts[0]?.rpm.toFixed(0)} RPM | tyre contacts ${contacts}/4\nTravel ${(this.state.distance/1000).toFixed(2)} km | origin transitions ${this.state.shifts} | local chart ${length(t.position).toFixed(1)} m\nStations ${regions.filter(r=>r.state==='active').length} active / ${regions.filter(r=>r.retained>0).length} retained | walking support ${world.entity(this.state.actor).character.supported}`;
  if(this.request){const s=saves.status(this.request);text+=`\nSave/load ${s?.state||''} ${s?.error||''}`;}if(this.problem&&!this.problem.includes('streaming transaction'))text+='\n'+this.problem;this.hud.get('status').text=text;
 }
 destroy(){scenes.removeInterest('traveller');input.pointerCapture=false;world.clearView();}
}
function physicsGravity(entity){return entity.gravity.acceleration;}
