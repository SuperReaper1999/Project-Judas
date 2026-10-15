import {world,input,ui,scenes,signals,saves} from 'judas';
import {CONTENT as C} from './content.js';
const v=(x=0,y=0,z=0)=>({x,y,z});
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
export default class Breakwater {
 constructor(){this.state={score:0,lives:3,hits:0,powers:0,phase:'ready',wideUntil:0,time:0};this.queue=[];this.request=0;this.finger=0;this.lastPointer=.5;this.menu=false;this.message='Launch when ready';}
 start(){this.bind();this.paddle=world.spawnPrefab(C.paddle_prefab,{position:v(0,-9.5,0)});this.readyBall();}
 restore(){this.bind();this.paddle=world.queryTags(['paddle'])[0];this.ball=world.queryTags(['ball'])[0];this.menu=false;this.message='Saved game restored';}
 bind(){this.doc=ui.get('breakwater');ui.debugOverlayVisible=false;input.pointerCapture=false;
  for(const name of ['hit','bounce','lost','power'])signals.subscribe('breakwater.'+name);
  this.pause(false);this.camera();this.hud();
 }
 camera(){const viewport=world.viewport,aspect=viewport.width/Math.max(1,viewport.height);const z=Math.max(36,10/Math.max(.2,aspect)/Math.tan(Math.PI*24/180));
  world.setView({position:v(0,0,z),rotation:{w:1,x:0,y:0,z:0}},48,{near:.1,far:120});
 }
 sound(name){try{world.entity(String(600+['hit','power','lost','launch'].indexOf(name))).playAudioOneShot();}catch(e){console.log('Audio: '+e.message);}}
 pause(value){this.menu=value;this.doc.modal=value;this.doc.get('shade').visible=value;this.doc.get('pause').visible=!value;this.doc.get('resume').visible=this.state.phase!=='won'&&this.state.phase!=='lost';}
 readyBall(){this.state.phase='ready';this.ball=world.spawnPrefab(C.ball_prefab,{position:v(this.paddle.transform.position.x,-8.8,0)});this.ball.setColliderEnabled(false);this.message='Tap LAUNCH or press Space';}
 launch(){if(this.state.phase!=='ready')return;this.ball.setColliderEnabled(true);this.ball.velocity=v(3.8,13,0);this.state.phase='playing';this.sound('launch');this.message='Catch gold capsules for power-ups';}
 uiUpdate(){if(input.pressed('restart')){scenes.reload();return;}if(!this.menu&&input.pressed('pause'))this.pendingPause=true;this.hud();}
 onUI(e){if(e.document!=='breakwater')return;if(e.type==='back'){if(this.menu)this.pause(false);else this.pause(true);return;}if(e.type!=='click')return;
  if(e.element==='pause')this.pause(true);else if(e.element==='resume')this.pause(false);else if(e.element==='restart')scenes.reload();else if(e.element==='quit')ui.quit();else if(['launch','save','load'].includes(e.element)){this.queue.push(e.element);if(e.element!=='launch')this.pause(false);}
 }
 fixedUpdate(dt){
  this.state.time+=dt;
  for(const command of this.queue.splice(0)){if(command==='launch')this.launch();else if(!this.request){try{this.request=command==='load'?saves.load('breakwater'):saves.save('breakwater',{name:'Breakwater / '+this.state.score});}catch(e){this.message=e.message;}}}
  if(input.pressed('launch'))this.launch();
  if(this.state.phase==='lost'||this.state.phase==='won')return;
  const p=this.paddle.transform.position;let x=p.x+input.axis('move')*22*dt;
  const touches=input.touches;let finger=touches.find(t=>t.id===this.finger);
  if(!finger){finger=touches.find(t=>t.y>.45&&t.y<.88);this.finger=finger?.id||0;}
  const pointer=input.pointer;
  let screenX=null;
  if(finger)screenX=finger.x;
  else if(pointer.down&&pointer.y>.35&&pointer.y<.88)screenX=pointer.x;
  if(screenX!==null){const left=world.project(v(-8,0,0)),right=world.project(v(8,0,0));if(left&&right)x=-8+16*(screenX-left.x)/(right.x-left.x);}
  const limit=this.state.wideUntil>this.state.time?5.5:6.45;
  x=clamp(clamp(x,-limit,limit),p.x-22*dt,p.x+22*dt);
  // Kinematic intent participates in the native fixed-step contact solver.
  this.paddle.moveKinematic({position:v(x,-9.5,0),rotation:{w:1,x:0,y:0,z:0}},dt);
  if(this.state.phase==='ready'&&this.ball?.valid)this.ball.transform={position:v(x,-8.8,0)};
  if(this.state.wideUntil&&this.state.time>=this.state.wideUntil){const pos=this.paddle.transform.position;this.paddle.destroy();this.paddle=world.spawnPrefab(C.paddle_prefab,{position:pos});this.state.wideUntil=0;}
 }
 update(){if(this.pendingPause){this.pendingPause=false;this.pause(true);}this.camera();if(this.request){const result=saves.status(this.request);if(result?.state==='completed'||result?.state==='failed'){this.message=result.state==='failed'?result.error:'Game saved on this device';this.request=0;}}}
 onSignal(event){
  if(event.name==='breakwater.hit'){
   const block=saves.resolve(event.payload.reference);if(!block?.valid)return;
   const p=block.transform.position;block.destroy();
   this.state.score+=100;this.state.hits++;this.sound('hit');
   if(this.state.hits%4===0){const drop=world.spawnPrefab(C.drop_prefab,{position:p});drop.velocity=v(0,-4,0);}
   if(world.queryTags(['brick']).length===0){this.state.phase='won';this.message='All clear!';this.doc.get('menu_title').text='CLEAN SWEEP';this.doc.get('menu_detail').text='Final score '+this.state.score;this.pause(true);}
  }else if(event.name==='breakwater.bounce')this.sound('hit');
  else if(event.name==='breakwater.lost'){
   if(this.state.phase!=='playing')return;
   if(world.queryTags(['ball']).length)return;
   this.state.lives--;this.sound('lost');
   if(this.state.lives<=0){this.state.phase='lost';this.message='No lives left';this.doc.get('menu_title').text='ONE MORE TRY?';this.doc.get('menu_detail').text='Final score '+this.state.score;this.pause(true);}else this.readyBall();
  }else if(event.name==='breakwater.power'){
   const mode=this.state.powers++%3;this.state.score+=50;this.sound('power');
   if(mode===0){const p=this.paddle.transform.position;this.paddle.destroy();this.paddle=world.spawnPrefab(C.wide_prefab,{position:p});this.state.wideUntil=this.state.time+15;this.message='WIDE PADDLE / 15 seconds';}
   else if(mode===1){const base=world.queryTags(['ball'])[0];if(base?.valid){const p=base.transform.position;for(const sign of [-1,1]){const b=world.spawnPrefab(C.ball_prefab,{position:v(p.x+sign*.7,p.y,0)});b.velocity=v(sign*7,12,0);}}this.message='MULTIBALL / three real bodies';}
   else{for(const b of world.queryTags(['ball'])){const s=b.velocity;b.applyImpulse(v(-s.x*.3*b.mass,-s.y*.3*b.mass,0));}this.message='SLOW BALL / impulse applied';}
  }
 }
 hud(){if(!this.doc)return;this.doc.get('score').text=String(this.state.score).padStart(5,'0')+'   /   '+this.state.lives+' LIVES';this.doc.get('notice').text=this.message;this.doc.get('launch').visible=this.state.phase==='ready'&&!this.menu;}
}
