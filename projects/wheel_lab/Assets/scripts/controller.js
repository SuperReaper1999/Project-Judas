// Project policy, not engine vehicle semantics. Every wheel receives direct
// torque, braking and steering intent during authoritative fixed updates.
import {world,input,ui,scenes,saves,session} from 'judas';
import {add,mul,length,rotate,qm,axis} from './math.js';

export const properties={autopilot:{type:'boolean',default:false}};
const buggy='81818181818181818181818100000006';
const payload='81818181818181818181818100000009';

export default class {
 constructor({properties}){
  this.props=properties;
  this.state={selected:0,spawned:[],payloads:[],grip:true,automatic:false,
              time:0,yaw:0,pitch:-.28,cockpit:false,region:false};
  this.pending=[];this.region='';this.request=0;
  this.message='W/S applies wheel torque, A/D steers, Space brakes. V lets the project drive a short watch tour.';
 }
 start(){
  this.hud=ui.get('lab');if(this.hud){this.hud.modal=false;this.hud.get('pause').visible=false;}
  input.pointerCapture=true;ui.debugOverlayVisible=false;
  this.state.automatic=!!(this.props.autopilot||session.get('wheelWatch'));
 }
 restore(){this.start();this.pending=[];this.region='';if(this.state.region)this.region=scenes.requestRegion('garage');}
 bodies(){return ['100','200',...this.state.spawned].map(id=>world.entity(id)).filter(e=>e?.valid);}
 current(){const bodies=this.bodies();return bodies[this.state.selected%bodies.length]||null;}
 wheels(body){return body?.children.filter(e=>e.valid&&e.wheel)||[];}
 update(dt){
  if(this.hud?.modal)return;
  for(const action of ['select','autopilot','payload','grip','spawn','region'])
   if(input.pressed(action))this.pending.push(action);
  this.state.yaw-=input.axis('look_x')*.002;
  this.state.pitch=Math.max(-.8,Math.min(.15,this.state.pitch-input.axis('look_y')*.002));
  if(input.pressed('view'))this.state.cockpit=!this.state.cockpit;
  if(input.pressed('flat'))scenes.load('Scenes/flat.judas');
  if(input.pressed('oblique'))scenes.load('Scenes/oblique.judas');
  if(input.pressed('radial'))scenes.load('Scenes/radial.judas');
  if(input.pressed('restart'))scenes.reload();
 }
 fixedUpdate(dt){
  for(const action of this.pending){
   if(action==='select'){this.state.selected=(this.state.selected+1)%this.bodies().length;this.state.yaw=0;}
   if(action==='autopilot'){
    this.state.automatic=!this.state.automatic;this.state.time=0;
    session.set('wheelWatch',this.state.automatic);
    this.message=this.state.automatic?'Watch tour: real torque over bumps, then brakes and reverse. V returns control.':'Manual control restored.';
   }
   if(action==='grip'){
    this.state.grip=!this.state.grip;
    for(const body of this.bodies())for(const wheel of this.wheels(body))wheel.wheel.configure({friction:this.state.grip?1:.02});
    this.message=this.state.grip?'Tyre friction restored to 1.0.':'Tyre friction 0.02: drive torque now produces much more slip.';
   }
   if(action==='payload'){
    if(this.state.payloads.length){for(const id of this.state.payloads)world.entity(id)?.destroy();this.state.payloads=[];
     this.message='225 kg payload removed; suspension responds to the actual unloading.';
    }else{const body=this.current();if(body){const t=body.transform;
     const box=world.spawnPrefab(payload,{position:add(t.position,rotate(t.rotation,{x:0,y:.75,z:.5})),rotation:t.rotation,scale:{x:1,y:1,z:1}});
     this.state.payloads=[box.id];this.message='225 kg ordinary rigid body placed on the rear deck. It has real contact and gravity.';
    }}
   }
   if(action==='spawn'&&this.state.spawned.length<3){const body=this.current();if(body){const t=body.transform;
    const assembly=world.spawnPrefab(buggy,{position:add(t.position,rotate(t.rotation,{x:6,y:.4,z:0})),rotation:t.rotation,scale:{x:1,y:1,z:1}});
    this.state.spawned.push(assembly.id);this.state.selected=this.bodies().length-1;
    this.message='Independent four-wheel prefab created. Tab switches between all current assemblies.';
   }}
   if(action==='region'){
    if(this.region){scenes.releaseRegion(this.region);this.region='';this.state.region=false;}
    else{this.region=scenes.requestRegion('garage');this.state.region=true;}
    this.message='Registered garage assembly '+(this.region?'requested.':'released.');
   }
  }
  this.pending=[];
  let throttle=input.axis('throttle'),steer=input.axis('steer')*.40,brake=input.held('brake')?750:0;
  if(this.state.automatic){
   this.state.time+=dt;const t=this.state.time%40;
   this.state.selected=t<20?0:1;
   const stage=t%20;
   throttle=stage>2&&stage<7?.8:stage>11&&stage<17?-.55:0;
   brake=stage>7&&stage<10||stage>17?650:0;
   steer=stage>6&&stage<7?.03:0;
  }
  const current=this.current();
  for(const body of this.bodies()){
   const selected=current?.id===body.id, speed=length(body.velocity);
   const command={driveTorque:selected&&speed<(this.state.automatic?6:10)?throttle*180:0,
                  brakeTorque:selected?brake:350,steeringAngle:0};
   const wheels=this.wheels(body);
   for(let i=0;i<wheels.length;++i){
    // This particular project steers its first axle. The engine knows only
    // independent mounts and never assumes where a front axle should be.
    wheels[i].wheel.set({...command,steeringAngle:selected&&i<2?steer:0});
   }
  }
 }
 presentationUpdate(){
  const body=this.current();if(!body)return;
  // Follow the same interpolated pose that Renderer presents, never a
  // fixed-step pose behind an interpolated vehicle model.
  const t=body.presentedTransform;
  const heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));
  const rotation=qm(heading,axis({x:1,y:0,z:0},this.state.cockpit?0:this.state.pitch));
  const position=add(t.position,rotate(heading,this.state.cockpit?{x:0,y:.95,z:-.72}:{x:0,y:3.2,z:7.8}));
  world.setView({position,rotation},67,{near:.1,far:250});
 }
 uiUpdate(){
  if(!this.hud)return;
  if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}
  if(input.pressed('save_game'))this.command('save');if(input.pressed('load_game'))this.command('load');
  if(this.request){const result=saves.status(this.request);if(result&&['completed','failed','cancelled'].includes(result.state)){
   this.message=result.state+(result.error?' — '+result.error:'');this.request=0;
  }}
  const body=this.current(),wheels=this.wheels(body),states=wheels.map(e=>e.wheel.state);
  this.hud.get('score').text=`${scenes.current.split('/').pop()} | ${wheels.length} independent wheels | chassis ${body?.mass||0} kg | ${length(body?.velocity||{x:0,y:0,z:0}).toFixed(2)} m/s | ${this.state.automatic?'WATCH TOUR':'MANUAL'}`;
  const contact=states.filter(s=>s.contact),lengths=states.map(s=>Math.round(s.length*1000)).join('/');
  const slip=states.reduce((maximum,s)=>Math.max(maximum,Math.abs(s.longitudinalSlip)),0);
  this.hud.get('progress').text=`Contacts ${contact.length}/${wheels.length} | travel ${lengths} mm | longitudinal slip ${slip.toFixed(2)} m/s | tyre friction ${this.state.grip?'1.0':'0.02'} | ${this.state.payloads.length?'225 kg payload':'no payload'}`;
  this.hud.get('message').text=states.some(s=>!s.available)?'A referenced chassis is unavailable; no wheel forces are applied.':this.message;
 }
 command(operation){if(!this.request)this.request=operation==='save'?saves.save('wheel-current',{name:'Wheel spin, commands and ordinary chassis state'}):operation==='load'?saves.load('wheel-current'):saves.delete('wheel-current');}
 onUI(event){
  if(event.type==='back'||event.type==='click'&&event.element==='resume'){
   this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;
  }
  if(event.type==='click'&&event.element==='restart')scenes.reload();
  if(event.type==='click'&&event.element==='quit')ui.quit();
  if(event.type==='click'&&['save','load','delete'].includes(event.element))this.command(event.element);
 }
 destroy(){if(this.region)scenes.releaseRegion(this.region);input.pointerCapture=false;world.clearView();}
}
