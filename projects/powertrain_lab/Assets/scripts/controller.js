// The project's driver, starter, gearbox and camera policy is ordinary JS.
// RPM and torque displays read solved shaft/contact state; they never drive it.
import {world,input,ui,scenes,saves,session} from 'judas';
import {add,length,rotate,qm,axis} from './math.js';

export const properties={autopilot:{type:'boolean',default:false}};
const buggy='82828282828282828282828200000006';
const payload='82828282828282828282828200000009';
const ratios=[1/4.5,1/3,1/2,1/1.3];

export default class {
 constructor({properties}){
  this.props=properties;
  this.state={selected:0,spawned:[],payloads:[],grip:true,automatic:false,
              time:0,yaw:0,pitch:-.28,cockpit:false,region:false,gear:0,
              clutch:true,machineLoad:false};
  this.pending=[];this.region='';this.request=0;
  this.message='W/S throttle, Q/E gear, F clutch, A/D steer, Space brake. Tab selects the electric rover or shaft machine.';
 }
 start(){
  this.hud=ui.get('lab');if(this.hud){this.hud.modal=false;this.hud.get('pause').visible=false;}
  input.pointerCapture=true;ui.debugOverlayVisible=false;
  this.state.automatic=!!(this.props.autopilot||session.get('powertrainWatch'));
 }
 restore(){this.start();this.pending=[];this.region='';if(this.state.region)this.region=scenes.requestRegion('garage');}
 assemblies(){return ['100','200','300',...this.state.spawned].map(id=>world.entity(id)).filter(e=>e?.valid&&e.powertrain);}
 current(){const bodies=this.assemblies();return bodies[this.state.selected%bodies.length]||null;}
 wheels(body){return body?.children.filter(e=>e.valid&&e.wheel)||[];}
 update(){
  if(this.hud?.modal)return;
  for(const action of ['select','autopilot','payload','grip','spawn','region','gear_down','gear_up','clutch'])
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
   if(action==='select'){this.state.selected=(this.state.selected+1)%this.assemblies().length;this.state.yaw=0;}
   if(action==='autopilot'){
    this.state.automatic=!this.state.automatic;this.state.time=0;
    session.set('powertrainWatch',this.state.automatic);
    this.message=this.state.automatic?'Watch: free-rev, clutch take-up, drive, brake; independent electric drive; then a loaded physical flywheel.':'Manual control restored.';
   }
   if(action==='gear_up')this.state.gear=Math.min(ratios.length-1,this.state.gear+1);
   if(action==='gear_down')this.state.gear=Math.max(0,this.state.gear-1);
   if(action==='clutch')this.state.clutch=!this.state.clutch;
   if(action==='grip'){
    this.state.grip=!this.state.grip;
    for(const body of this.assemblies())for(const wheel of this.wheels(body))wheel.wheel.configure({friction:this.state.grip?1:.02});
    this.message=this.state.grip?'Tyre friction restored to 1.0.':'Tyre friction 0.02; differential and rotor loads respond to actual tyre slip.';
   }
   if(action==='payload'){
    const body=this.current();
    if(!this.wheels(body).length){this.state.machineLoad=!this.state.machineLoad;this.message='Physical shaft brake load '+(this.state.machineLoad?'applied.':'released.');}
    else if(this.state.payloads.length){for(const id of this.state.payloads)world.entity(id)?.destroy();this.state.payloads=[];this.message='225 kg payload removed.';}
    else if(body){const t=body.transform;const box=world.spawnPrefab(payload,{position:add(t.position,rotate(t.rotation,{x:0,y:.75,z:.5})),rotation:t.rotation,scale:{x:1,y:1,z:1}});
     this.state.payloads=[box.id];this.message='225 kg ordinary rigid body placed on the rear deck; load reaches wheels and powertrain.';}
   }
   if(action==='spawn'&&this.state.spawned.length<3){const body=this.current();if(body){const t=body.transform;
    const assembly=world.spawnPrefab(buggy,{position:add(t.position,rotate(t.rotation,{x:6,y:.4,z:0})),rotation:t.rotation,scale:{x:1,y:1,z:1}});
    this.state.spawned.push(assembly.id);this.state.selected=this.assemblies().length-1;
    this.message='Complete engine, transmission, differential and wheel prefab created with remapped references.';
   }}
   if(action==='region'){
    if(this.region){scenes.releaseRegion(this.region);this.region='';this.state.region=false;}
    else{this.region=scenes.requestRegion('garage');this.state.region=true;}
    this.message='Registered mechanical garage '+(this.region?'requested.':'released.');
   }
  }
  this.pending=[];
  let throttle=input.axis('throttle'),steer=input.axis('steer')*.40,
      braking=input.held('brake')?1:0,clutch=this.state.clutch?1:0,gear=this.state.gear;
  if(this.state.automatic){
   this.state.time+=dt;const t=this.state.time%66;
   this.state.selected=Math.floor(t/22);const stage=t%22;
   throttle=stage>2&&stage<12?.65:stage>16&&stage<20?-.45:0;
   braking=stage<2||stage>12&&stage<16||stage>20?1:0;
   clutch=stage<6?0:Math.min(1,(stage-6)/1.2);
   gear=stage>10?1:0;steer=stage>10&&stage<11?.04:0;
  }
  const current=this.current();
  for(const assembly of this.assemblies()){
   const selected=current?.id===assembly.id,graph=assembly.powertrain.settings,
         state=assembly.powertrain.state,wheels=this.wheels(assembly);
   const engine=graph.sources.some(s=>s.kind==='combustion');
   if(engine){
    const crank=state.shafts.find(s=>s.name==='crank');
    const throttleMagnitude=selected?Math.abs(throttle):0;
    assembly.powertrain.set({
     sources:[{name:'engine',input:Math.max(.08,throttleMagnitude),starter:crank&&Math.abs(crank.rpm)<850?1:0}],
     links:[{name:'gearbox',ratio:ratios[gear]*(selected&&throttle<0?-1:1)},
            {name:'clutch',engagement:selected?clutch:0}],
     brakes:[{name:'left',engagement:selected?braking:1},{name:'right',engagement:selected?braking:1}]
    });
   }else{
    const isMachine=!wheels.length;
    assembly.powertrain.set({
     sources:graph.sources.map(s=>({name:s.name,input:selected?throttle:0})),
     brakes:graph.brakes.map(b=>({name:b.name,engagement:selected?Math.max(braking,isMachine&&this.state.machineLoad ? .45 : 0):1}))
    });
   }
   for(let i=0;i<wheels.length;++i){
    // Independent direct wheel control remains available; this project uses
    // steering and unpowered front-wheel brakes here, not drive torque.
    const frontBrake=engine&&i<2?(selected?braking:1)*750:0;
    wheels[i].wheel.set({driveTorque:0,brakeTorque:frontBrake,steeringAngle:selected&&i<2?steer:0});
   }
  }
 }
 presentationUpdate(){
  const body=this.current();if(!body)return;
  const t=body.presentedTransform;
  const machine=!this.wheels(body).length;
  const heading=qm(t.rotation,axis({x:0,y:1,z:0},this.state.yaw));
  const cameraHeading=machine?qm(heading,axis({x:0,y:1,z:0},Math.atan2(4,6))):heading;
  const rotation=qm(cameraHeading,axis({x:1,y:0,z:0},this.state.cockpit&&!machine?0:this.state.pitch));
  const position=add(t.position,rotate(heading,this.state.cockpit&&!machine?{x:0,y:.95,z:-.72}:machine?{x:4,y:3.2,z:6}:{x:0,y:3.2,z:7.8}));
  world.setView({position,rotation},67,{near:.1,far:250});
 }
 uiUpdate(){
  if(!this.hud)return;
  if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}
  if(input.pressed('save_game'))this.command('save');if(input.pressed('load_game'))this.command('load');
  if(this.request){const result=saves.status(this.request);if(result&&['completed','failed','cancelled'].includes(result.state)){
   this.message=result.state+(result.error?' — '+result.error:'');this.request=0;
  }}
  const body=this.current(),power=body?.powertrain?.state;if(!power)return;
  const shafts=power.shafts.map(s=>`${s.name} ${Math.round(s.rpm)} RPM`).join(' | ');
  const wheelCount=this.wheels(body).length,
        torque=power.sources.reduce((n,s)=>n+Math.abs(s.torque),0),
        slip=power.links.reduce((n,s)=>Math.max(n,Math.abs(s.slip)),0),
        clutch=power.links.find(s=>s.name==='clutch'),
        title=clutch?'COMBUSTION TRANSMISSION':wheelCount?'INDEPENDENT ELECTRIC ROVER':'PHYSICAL SHAFT MACHINE',
        motion=wheelCount?`${length(body.velocity).toFixed(2)} m/s`:`${Math.round(power.shafts.find(s=>s.name==='flywheel')?.rpm||0)} rotor RPM`;
  this.hud.get('score').text=`${scenes.current.split('/').pop()} | ${title} | ${motion} | ${this.state.automatic?'WATCH':'MANUAL'}`;
  this.hud.get('progress').text=`${shafts} | source ${torque.toFixed(1)} Nm | link slip ${slip.toFixed(2)} rad/s${clutch?' | clutch '+(clutch.active?'active':'open'):''}`;
  this.hud.get('message').text=!power.available?power.diagnostic||'A required mechanical endpoint is unavailable; its connections are inactive.':this.message;
 }
 command(operation){if(!this.request)this.request=operation==='save'?saves.save('powertrain-current',{name:'Shaft momentum, commands and physical endpoints'}):operation==='load'?saves.load('powertrain-current'):saves.delete('powertrain-current');}
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
