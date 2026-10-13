// Project policy only: input, ordinary loads, watch sequence, cameras and UI.
// Material geometry, mounting failure and drive continuity are engine results.
import {world,input,ui,scenes,saves,session} from 'judas';
import {add,rotate,qm,axis} from './math.js';

export const properties={autopilot:{type:'boolean',default:false}};
const prefab='8383838383838383838383830000000a';
const ramPrefab='8383838383838383838383830000000d';
const ids=['100','200','300','500','700'];
const scalar=(value,low,high)=>Math.max(low,Math.min(high,value));

export default class {
 constructor({properties}){
  this.props=properties;
  this.state={selected:0,load:false,overload:false,automatic:false,time:0,
              yaw:0,pitch:-.30,spawned:[],rams:[],region:false};
  this.pending=[];this.region='';this.request=0;
  this.message='Orange yields; green has the same geometry with stronger material and mounts. Purple loses its cohesive load path.';
 }
 start(){
  this.hud=ui.get('lab');if(this.hud){this.hud.modal=false;this.hud.get('pause').visible=false;}
  this.state.automatic=!!(this.state.automatic||this.props.autopilot||session.get('damageWatch'));
  input.pointerCapture=true;ui.debugOverlayVisible=false;
 }
 restore(){this.start();this.pending=[];this.region='';if(this.state.region)this.region=scenes.requestRegion('annex');}
 assemblies(){return [...ids,...this.state.spawned].map(id=>world.entity(id)).filter(e=>e?.valid&&e.powertrain);}
 current(){const all=this.assemblies();return all[this.state.selected%all.length]||null;}
 arms(owner){return owner.children.filter(e=>e.valid&&e.deformable);}
 carriers(owner){return owner.children.filter(e=>e.valid&&e.children.some(c=>c.wheel));}
 update(){
  if(this.hud?.modal)return;
  for(const action of ['select','autopilot','load','impact','overload','spawn','region'])
   if(input.pressed(action))this.pending.push(action);
  this.state.yaw-=input.axis('look_x')*.002;
  this.state.pitch=scalar(this.state.pitch-input.axis('look_y')*.002,-.9,.15);
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
    session.set('damageWatch',this.state.automatic);this.state.load=false;this.state.overload=false;
    this.message=this.state.automatic?'Watch: equal bearing loads, unload, physical impacts, then overload and inspect disconnected drive.':'Manual controls restored. R reloads the authored intact state.';
   }
   if(action==='load'){this.state.load=!this.state.load;this.message=this.state.load?'Equal 60 N side loads applied to each bearing; no deformation is prescribed.':'Load removed. Compare permanent orange misalignment and green recovery.';}
   if(action==='overload'){this.state.overload=!this.state.overload;this.message=this.state.overload?'Equal 450 N overload applied; actual mounting reactions and cohesive strengths decide failure.':'Overload removed; broken mounts and material stay broken.';}
   if(action==='impact'){const owner=this.current();if(owner)this.impact(owner);}
   if(action==='spawn'&&this.state.spawned.length<2){
    const frame=world.entity('500').transform;
    const assembly=world.spawnPrefab(prefab,{position:add(frame.position,rotate(frame.rotation,{x:-7+this.state.spawned.length*6,y:0,z:6})),rotation:frame.rotation,scale:{x:1,y:1,z:1}});
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
   this.state.selected=t<6?0:t<12?1:t<20?2:t<28?3:4;
   throttle=.4;braking=0;
   loading=t>2&&t<7?60:t>14&&t<18?450:0;
   if(old<10&&t>=10)for(const assembly of this.assemblies().slice(0,3))this.impact(assembly,10);
   if(old<20&&t>=20)for(const assembly of this.assemblies().slice(3,5))this.impact(assembly,35);
   if(t>36){this.state.automatic=false;session.set('damageWatch',false);this.message='Watch complete. Inspect the real failed mounts and retained material; R starts a fresh authored scene.';}
  }
  const selected=this.current();
  for(const assembly of this.assemblies()){
   const settings=assembly.powertrain.settings;
   const powered=this.state.automatic||selected?.id===assembly.id;
   assembly.powertrain.set({sources:settings.sources.map(s=>({name:s.name,input:powered?throttle:0})),
     brakes:settings.brakes.map(b=>({name:b.name,engagement:powered?braking:1}))});
   const carriers=this.carriers(assembly);
   for(let index=0;index<carriers.length;++index){
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
  const label=['Plastic mount','Strong control','Cohesive material path','Plastic buggy','Strong buggy'][ids.indexOf(current.id)]||'Independent prefab';
  this.hud.get('score').text=`${scenes.current.split('/').pop()} | ${label} | ${this.state.automatic?'WATCH':'MANUAL'}`;
  this.hud.get('progress').text=`${shafts} | connected drive ${links}/${state.links.length} | released ${failed} | fractured ${broken} | strain ${Math.max(0,...strains).toFixed(3)}`;
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
