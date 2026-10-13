// Project policy only: input, ordinary loads, watch sequence, cameras and UI.
// Material geometry, mounting failure and drive continuity are engine results.
import {world,input,ui,scenes,saves,session} from 'judas';
import {add,rotate,qm,axis} from './math.js';

export const properties={autopilot:{type:'boolean',default:false},crashDemo:{type:'boolean',default:false},vehicleDemo:{type:'boolean',default:false}};
const prefab='8383838383838383838383830000000a';
const crashPrefab='8383838383838383838383830000000e';
const vehiclePrefab='83838383838383838383838300000012';
const ramPrefab='8383838383838383838383830000000d';
const ids=['100','200','300','500','700'];
const scalar=(value,low,high)=>Math.max(low,Math.min(high,value));

export default class {
 constructor({properties}){
  this.props=properties;
  this.state={selected:0,load:false,overload:false,automatic:false,time:0,
              yaw:0,pitch:-.30,spawned:[],rams:[],region:false,impactTime:-1};
  this.pending=[];this.region='';this.request=0;this.families=new Map();
  this.message='Orange yields; green has the same geometry with stronger material and mounts. Purple loses its cohesive load path.';
 }
 start(){
  this.hud=ui.get('lab');if(this.hud){this.hud.modal=false;this.hud.get('pause').visible=false;}
  this.state.automatic=!!(this.state.automatic||this.props.autopilot||session.get('damageWatch'));
  if(this.props.crashDemo){
   this.message='V starts the wall crash: orange has weaker mounts; green has stronger mounts.';
   if(this.hud){this.hud.get('title').text='JUDAS / CRASH LANE';
    this.hud.get('controls').text='V watch | W/S drive | A/D steer | Space brake | Tab compare | 1/2/3 scenes | F5/F8 save/load | R reset | Esc pause';}
  }
  if(this.props.vehicleDemo){
   this.message='V starts the crash comparison: actual front structure can crumple and fracture.';
   if(this.hud){this.hud.get('title').text='JUDAS / STRUCTURAL CRASH';
    this.hud.get('controls').text='V watch | W/S drive | A/D steer | Space brake | Tab compare | 1 new / 2 rigs / 3 rotated / 4 prior | F5/F8 save | R reset | Esc pause';}
  }
  input.pointerCapture=true;ui.debugOverlayVisible=false;
 }
 isCar(){return !!(this.props.crashDemo||this.props.vehicleDemo);}
 restore(){this.families=new Map();this.start();this.pending=[];this.region='';if(this.state.region)this.region=scenes.requestRegion('annex');}
 assemblies(){
  const all=[...(this.isCar()?['900','1100']:ids),...this.state.spawned].map(id=>world.entity(id));
  if(!this.isCar()){const annex=scenes.resolveRegionEntity('annex','1');if(annex)all.push(annex);}
  return all.filter(e=>e?.valid&&e.powertrain);
 }
 current(){const all=this.assemblies();return all[this.state.selected%all.length]||null;}
 carFamily(owner){
  let family=this.families.get(owner.id);
  if(!family){
   // This authored prefab has a fixed hierarchy. Keep safe handles instead of
   // enumerating/copying the scene for every visual child on every callback.
   // A fresh script/restore rebuilds this cache; poses and physical state are
   // always read live. Runtime prefab instances get independent entries.
   const children=owner.children;
   const modules=children.filter(e=>e.valid&&e.collider!==null);
   const wheels=[...children.filter(e=>e.wheel),...modules.flatMap(e=>e.children.filter(c=>c.wheel))];
   family={children,modules,wheels:wheels.map(e=>({entity:e,handle:e.wheel,front:e.wheel.settings.chassisId!==owner.id})),materials:[],settings:owner.powertrain.settings};
   this.families.set(owner.id,family);
  }
  // Normal asynchronous loading can publish the material after the script.
  if(!family.materials.length)family.materials=family.children.filter(e=>e.valid&&e.deformable);
  return family;
 }
 arms(owner){return this.props.vehicleDemo?this.carFamily(owner).materials.filter(e=>e.valid):owner.children.filter(e=>e.valid&&e.deformable);}
 carriers(owner){return this.props.vehicleDemo?this.carFamily(owner).modules.filter(e=>e.valid):owner.children.filter(e=>e.valid&&e.children.some(c=>c.wheel));}
 update(){
  if(this.hud?.modal)return;
  for(const action of ['select','autopilot','load','impact','overload','spawn','region'])
   if(input.pressed(action))this.pending.push(action);
  this.state.yaw-=input.axis('look_x')*.002;
  this.state.pitch=scalar(this.state.pitch-input.axis('look_y')*.002,-.9,.15);
  if(input.pressed('vehicle'))scenes.load('Scenes/vehicle_crash.judas');
  if(input.pressed('crash'))scenes.load('Scenes/crash.judas');
  if(input.pressed('flat'))scenes.load('Scenes/flat.judas');
  if(input.pressed('oblique'))scenes.load('Scenes/oblique.judas');
  if(input.pressed('restart'))scenes.reload();
 }
 impact(owner, magnitude=10){
  const carrier=this.carriers(owner)[0];if(!carrier)return;
  const q=owner.transform.rotation;
  const ram=world.spawnPrefab(ramPrefab,{position:add(carrier.transform.position,rotate(q,{x:0,y:.1,z:1.5})),rotation:q,scale:{x:1,y:1,z:1}});
  ram.applyImpulse(rotate(q,{x:0,y:0,z:-magnitude*10}));this.state.rams.push(ram.id);
  // The bounded content cleanup only removes old, separately spawned strikers.
  // It never alters the material, carrier or mechanical link.
  while(this.state.rams.length>6){const old=world.entity(this.state.rams.shift());if(old?.valid)old.destroy();}
  this.message='Ordinary 12 kg ram launched at a finite bearing. Real contact, material and mount strength resolve the impact.';
 }
 fixedUpdate(dt){
  for(const action of this.pending){
   if(action==='select'){this.state.selected=(this.state.selected+1)%this.assemblies().length;this.state.yaw=0;}
   if(action==='autopilot'){
    this.state.automatic=!this.state.automatic;this.state.time=0;
    this.state.impactTime=-1;
    session.set('damageWatch',this.state.automatic);this.state.load=false;this.state.overload=false;
    this.message=this.state.automatic?(this.props.crashDemo?'1 / INTACT: same geometry and drive input; different suspension-mount strength.':'Watch: equal bearing loads, unload, physical impacts, then overload and inspect disconnected drive.'):'Manual controls restored. R reloads the authored intact state.';
   }
   if(action==='load'){this.state.load=!this.state.load;this.message=this.state.load?'Equal 60 N side loads applied to each bearing; no deformation is prescribed.':'Load removed. Compare permanent orange misalignment and green recovery.';}
   if(action==='overload'){this.state.overload=!this.state.overload;this.message=this.state.overload?'Equal 450 N overload applied; actual mounting reactions and cohesive strengths decide failure.':'Overload removed; broken mounts and material stay broken.';}
   if(action==='impact'){const owner=this.current();if(owner)this.impact(owner);}
   if(action==='spawn'&&this.state.spawned.length<2){
    const frame=world.entity(this.isCar()?'900':'500').transform;
    const assembly=world.spawnPrefab(this.props.vehicleDemo?vehiclePrefab:this.props.crashDemo?crashPrefab:prefab,{position:add(frame.position,rotate(frame.rotation,{x:-7+this.state.spawned.length*6,y:0,z:6})),rotation:frame.rotation,scale:{x:1,y:1,z:1}});
    this.state.spawned.push(assembly.id);this.message='Complete coupled suspension prefab created; material and mechanical references use normal remapping.';
   }
   if(action==='region'){
    if(this.region){scenes.releaseRegion(this.region);this.region='';this.state.region=false;}
    else{this.region=scenes.requestRegion('annex');this.state.region=true;}
    this.message='Retained coupled bearing region '+(this.region?'requested.':'released.');
   }
  }
  this.pending=[];
  let throttle=input.axis('throttle'),braking=input.held('brake')?1:0,
      loading=this.state.overload?450:this.state.load?60:0;
  if(this.state.automatic){
   const old=this.state.time;this.state.time+=dt;const t=this.state.time;
   if(this.isCar()){
    // This tour supplies ordinary driver commands. It never launches a ram,
    // sets a failure flag or prescribes car/wheel motion. The real wall and
    // physical mounts decide the outcome; observation controls narration only.
    const weak=world.entity('900');
    const damaged=this.props.vehicleDemo?
     this.arms(weak).some(e=>e.fracture?.state.interfaces.some(i=>i.broken&&i.cause==='physical'))&&weak.powertrain.state.links.slice(0,2).some(l=>!l.connected):
     this.arms(weak).some(e=>e.deformable.state.attachments.some(a=>a.released));
    if(this.state.impactTime<0&&damaged)this.state.impactTime=t;
    const after=this.state.impactTime<0?-1:t-this.state.impactTime;
    // Release throttle and apply ordinary mechanical brakes after the
    // observed crash so the viewer can inspect the physical result.
    throttle=t<3||after>2?0:1;braking=t<3||after>2?1:0;loading=0;
    this.state.selected=after>=10?1:0;
    this.message=t<3?'1 / INTACT: identical cars, equal inputs, different mounting strength.':
     after<0?'2 / APPROACH: engine torque and tyre traction drive both cars into solid walls.':
     after<6?'3 / IMPACT: orange lost a wheel mount. The wheel keeps its own physical motion.':
     after<10?'4 / DAMAGE: broken mount = open drive path. The detached wheel still collides.':
     '5 / GREEN: same crash input; stronger mounts kept its wheel connections intact.';
    if(this.props.vehicleDemo)this.message=t<3?'1 / INTACT: equal cars and drive input; different structural strength.':
     after<0?'2 / APPROACH: the front structure carries real load between the cabin and axle.':
     after<6?'3 / IMPACT: internal front-structure connections broke under collision load.':
     after<10?'4 / DAMAGE: inspect the separated front module, deformed material and lost front drive.':
     '5 / COMPARISON: stronger internal connections change the physical result.';
    if(after>18||t>30){this.state.automatic=false;session.set('damageWatch',false);throttle=0;braking=1;
     this.message=after<0?'No suspension failure observed. Inspect the physical result; R resets the cars.':'Crash tour complete. Tab compares cars; W/S tests remaining drive. R resets the intact assembly.';}
   }else{
    this.state.selected=t<6?0:t<12?1:t<20?2:t<28?3:4;
    throttle=.4;braking=0;
    loading=t>2&&t<7?60:t>14&&t<18?450:0;
    if(old<10&&t>=10)for(const assembly of this.assemblies().slice(0,3))this.impact(assembly,10);
    if(old<20&&t>=20)for(const assembly of this.assemblies().slice(3,5))this.impact(assembly,35);
    if(t>36){this.state.automatic=false;session.set('damageWatch',false);this.message='Watch complete. Inspect the real failed mounts and retained material; R starts a fresh authored scene.';}
   }
  }
  const selected=this.current();
  for(const assembly of this.assemblies()){
   const settings=this.props.vehicleDemo?this.carFamily(assembly).settings:assembly.powertrain.settings;
   const powered=this.isCar()||this.state.automatic||selected?.id===assembly.id;
   assembly.powertrain.set({sources:settings.sources.map(s=>({name:s.name,input:powered?throttle:0})),
     brakes:settings.brakes.map(b=>({name:b.name,engagement:powered?braking:1}))});
   const carriers=this.carriers(assembly);
   if(this.props.vehicleDemo){
    // The front wheels belong to the actual front rigid module; rear wheels
    // act on the cabin body. Commands do not move a wheel or mend a structure.
    for(const wheel of this.carFamily(assembly).wheels)if(wheel.entity.valid&&wheel.handle.valid)wheel.handle.set({driveTorque:0,brakeTorque:0,
     steeringAngle:wheel.front&&!this.state.automatic?input.axis('steer')*.3:0});
   }
   for(let index=0;!this.props.vehicleDemo&&index<carriers.length;++index){
    const carrier=carriers[index];
    if(loading)carrier.applyForce(rotate(assembly.transform.rotation,{x:0,y:0,z:loading}));
    for(const wheel of carrier.children.filter(c=>c.wheel))wheel.wheel.set({driveTorque:0,brakeTorque:0,
      steeringAngle:index<2&&!this.state.automatic?input.axis('steer')*.3:0});
   }
  }
 }
 presentationUpdate(){
  const current=this.current();if(!current)return;
  const transform=current.presentedTransform,station=this.carriers(current).length===1;
  if(this.isCar()){
   // The authored gravity frame keeps the camera horizon independent of a
   // crashing body's roll. Only the camera is staged; simulation is unchanged.
   const frame=world.entity('1').presentedTransform;
   const t=this.state.time,after=this.state.impactTime<0?-1:t-this.state.impactTime;
   if(this.state.automatic&&t<3){
    const rotation=qm(frame.rotation,qm(axis({x:0,y:1,z:0},.65),axis({x:1,y:0,z:0},-.32)));
    world.setView({position:add(frame.position,rotate(frame.rotation,{x:14,y:8,z:22})),rotation},65,{near:.08,far:250});return;
   }
   const side=this.props.vehicleDemo?(this.state.automatic?(after>=6?1.50:1.25):this.state.yaw+1.25):this.state.automatic?(after >= 6 ? .95 : .45):this.state.yaw+.45;
   const heading=qm(frame.rotation,axis({x:0,y:1,z:0},side));
   const rotation=qm(heading,axis({x:1,y:0,z:0},this.state.automatic?-.30:this.state.pitch));
   const distance=this.state.automatic&&after>=6?5:7;
   let focus=transform.position;
   if(this.props.vehicleDemo){const front=this.carriers(current)[0];
    if(front?.valid){const nose=front.presentedTransform.position;
     focus={x:(focus.x+nose.x)*.5,y:(focus.y+nose.y)*.5,z:(focus.z+nose.z)*.5};}}
   world.setView({position:add(focus,rotate(heading,{x:0,y:this.props.vehicleDemo?2.0:2.7,z:distance})),rotation},60,{near:.08,far:250});return;
  }
  const heading=qm(transform.rotation,axis({x:0,y:1,z:0},this.state.yaw+(station ? .40 : 0)));
  const rotation=qm(heading,axis({x:1,y:0,z:0},this.state.pitch));
  const position=add(transform.position,rotate(heading,station?{x:1.1,y:2.1,z:6.1}:{x:0,y:3,z:7}));
  world.setView({position,rotation},67,{near:.08,far:250});
 }
 uiUpdate(){
  if(!this.hud)return;
  if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}
  if(input.pressed('save_game'))this.command('save');if(input.pressed('load_game'))this.command('load');
  if(this.request){const result=saves.status(this.request);if(result&&['completed','failed','cancelled'].includes(result.state)){
   this.message=result.state+(result.error?' — '+result.error:'');this.request=0;
  }}
  const current=this.current(),state=current?.powertrain.state;if(!state)return;
  const arms=this.arms(current),strains=arms.map(e=>e.deformable.state.maximumStrain),
        failed=arms.reduce((sum,e)=>sum+(e.deformable.state.attachments||[]).filter(a=>a.released).length,0),
        broken=arms.reduce((sum,e)=>sum+(e.fracture?.state.interfaces.filter(i=>i.broken).length||0),0),
        links=state.links.filter(link=>link.active).length,
        shafts=state.shafts.map(s=>s.name+' '+Math.round(s.rpm)+' RPM').join(' | '),
        errors=arms.map(e=>e.deformable.state.error).filter(Boolean);
  const label=(this.props.vehicleDemo?{'900':'ORANGE / yielding front structure','1100':'GREEN / stronger structure'}[current.id]:this.props.crashDemo?{'900':'ORANGE / weak mounts','1100':'GREEN / strong mounts'}[current.id]:['Plastic mount','Strong control','Cohesive material path','Plastic buggy','Strong buggy'][ids.indexOf(current.id)])
      ||(scenes.owner(current)==='annex'?'Retained annex bearing':'Independent prefab');
  this.hud.get('score').text=`${scenes.current.split('/').pop()} | ${label} | ${this.state.automatic?'WATCH':'MANUAL'}`;
  this.hud.get('progress').text=this.props.crashDemo?`Speed ${Math.hypot(current.velocity.x,current.velocity.y,current.velocity.z).toFixed(1)} m/s | drive paths ${state.links.filter(l=>l.connected).length}/4 | failed mounts ${failed} | ${Math.round(state.shafts[0].rpm)} motor RPM`:
   `${shafts} | connected drive ${links}/${state.links.length} | released ${failed} | fractured ${broken} | strain ${Math.max(0,...strains).toFixed(3)}`;
  if(this.props.vehicleDemo)this.hud.get('progress').text=`Speed ${Math.hypot(current.velocity.x,current.velocity.y,current.velocity.z).toFixed(1)} m/s | front drive ${state.links.slice(0,2).filter(l=>l.connected).length}/2 | broken sections ${broken} | strain ${Math.max(0,...strains).toFixed(2)}`;
  this.hud.get('message').text=errors[0]||(!state.converged?state.diagnostic:this.message);
 }
 command(operation){if(!this.request)this.request=operation==='save'?saves.save('damage-current',{name:'Bent material, failed mounts, physical carriers and mechanical drive'}):saves.load('damage-current');}
 onUI(event){
  if(event.type==='back'||event.type==='click'&&event.element==='resume'){
   this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;
  }
  if(event.type==='click'&&event.element==='restart')scenes.reload();
  if(event.type==='click'&&event.element==='quit')ui.quit();
  if(event.type==='click'&&['save','load'].includes(event.element))this.command(event.element);
  if(event.type==='click'&&event.element==='delete')this.request=saves.delete('damage-current');
 }
 destroy(){if(this.region)scenes.releaseRegion(this.region);input.pointerCapture=false;world.clearView();}
}
