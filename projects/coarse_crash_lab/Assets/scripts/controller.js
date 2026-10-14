// Driver, camera, controls and UI only. Material history and failed driveline
// connections are engine results. No body pose, damage or crush is prescribed.
import {world,input,ui,scenes,saves,session} from 'judas';
import {add,sub,dot,rotate,inverse,qm,axis,length} from './math.js';
export const properties={case:{type:'string',default:'medium'},autopilot:{type:'boolean',default:false}};
const cases=['normal','low','medium','high','offset','comparison','rotated','group','specimen'];
const target={normal:10,low:5,medium:15,high:25,offset:15,comparison:15,rotated:15,group:15,group_high:25};
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
export default class {
 constructor({properties}){this.props=properties;this.state={automatic:false,time:0,selected:0,spawned:[],yaw:.85,pitch:-.24,debug:false,hit:{},lanes:{},load:false,overload:false};this.pending=[];this.request=0;this.region='';this.message='V starts real torque-driven driving into the wall.';}
 start(){this.hud=ui.get('lab');if(this.hud){this.hud.modal=false;this.hud.get('pause').visible=false;}this.state.automatic=!!(this.props.autopilot||this.state.automatic||session.get('coarseTour'));input.pointerCapture=true;ui.debugOverlayVisible=false;}
 restore(){this.pending=[];this.request=0;this.region='';this.start();}
 cars(){const n=this.props.case.startsWith('group')?4:this.props.case==='comparison'?3:1;const ids=Array.from({length:n},(_,i)=>String(900+300*i));const result=[...ids,...this.state.spawned].map(id=>world.entity(id)).filter(e=>e?.valid);const annex=scenes.resolveRegionEntity('annex','900');if(annex)result.push(annex);return result;}
 current(){if(this.props.case==='specimen')return world.entity('500');const all=this.cars();return all[this.state.selected%all.length];}
 material(car){return car.children.find(e=>e.deformable)?.fracture;}
 speed(car){if(this.props.case==='specimen')return 0;return dot(car.velocity,rotate(world.entity('1').transform.rotation,{x:0,y:0,z:-1}));}
 update(){if(this.hud?.modal)return;for(const a of ['watch','select','spawn','tour','region','load','overload'])if(input.pressed(a))this.pending.push(a);for(const c of cases)if(input.pressed(c)){session.set('coarseTour',null);scenes.load('Scenes/'+c+'.judas');}if(input.pressed('restart'))scenes.reload();if(input.pressed('contacts')){this.state.debug=!this.state.debug;world.debugView={collisionShapes:this.state.debug,contacts:this.state.debug,wheels:this.state.debug,deformables:this.state.debug};}this.state.yaw-=input.axis('look_x')*.002;this.state.pitch=clamp(this.state.pitch-input.axis('look_y')*.002,-.8,.15);}
 fixedUpdate(dt){
  for(const a of this.pending){if(a==='watch'){this.state.automatic=!this.state.automatic;this.state.time=0;}
   if(a==='select')this.state.selected++;if(a==='load')this.state.load=!this.state.load;if(a==='overload')this.state.overload=!this.state.overload;
   if(a==='tour'){session.set('coarseTour',0);scenes.load('Scenes/low.judas');}
   if(a==='spawn'&&this.props.case!=='specimen'){const car=this.current();const e=world.spawnPrefab('83838383838383838383c83d00000009',{position:add(car.transform.position,rotate(world.entity('1').transform.rotation,{x:5,y:0,z:0})),rotation:world.entity('1').transform.rotation,scale:{x:1,y:1,z:1}});this.state.spawned.push(e.id);}
   if(a==='region'){if(this.region){scenes.releaseRegion(this.region);this.region='';}else this.region=scenes.requestRegion('annex');}
  }this.pending=[];this.state.time+=dt;
  if(this.props.case==='specimen'){const specimen=world.entity('520')?.fracture;const t=this.state.time;const force=this.state.automatic?(t>=2&&t<4?20:t>=8&&t<10?400:t>=16&&t<18?2000:0):this.state.overload?400:this.state.load?20:0;if(force&&specimen)specimen.force(0,rotate(world.entity('1').transform.rotation,{x:0,y:-force,z:0}));this.message='B mild load / H overload / V watches elastic recovery, permanent bend and physical failure';return;}
  const cars=this.cars(),selected=this.current();
  for(const car of cars){const isSelected=car.id===selected?.id,frame=world.entity('1').transform;
   let drive=isSelected?input.axis('throttle'):0,brake=isSelected&&input.held('brake')?1:0,steer=isSelected?input.axis('steer')*.24:0;
   if(this.state.automatic){const speed=this.speed(car),z=rotate(inverse(frame.rotation),sub(car.transform.position,frame.position)).z;
    if(z<4&&speed<target[this.props.case]*.8)this.state.hit[car.id]=true;
    const localPosition=rotate(inverse(frame.rotation),sub(car.transform.position,frame.position));
    if(this.state.lanes[car.id]===undefined)this.state.lanes[car.id]=localPosition.x;
    const forward=rotate(inverse(frame.rotation),rotate(car.transform.rotation,{x:0,y:0,z:-1}));
    const heading=Math.atan2(forward.x,-forward.z),desired=Math.atan2(this.state.lanes[car.id]-localPosition.x,12);
    steer=clamp((heading-desired)*1.3,-.25,.25);
    const wanted=this.state.time<2||this.state.hit[car.id]?0:target[this.props.case];
    drive=wanted?clamp((wanted-speed)*.45,0,1):0;brake=wanted?clamp((speed-wanted)*.3,0,.5):1;
    if(this.props.case==='normal'&&this.state.time>8)steer+=.04*Math.sin(this.state.time*.7);
   }
   car.powertrain.set({sources:[{name:'motor',input:Math.max(0,drive)}],brakes:[1,2,3,4].map(i=>({name:'wheel'+i,engagement:brake}))});
   for(const child of car.children)if(child.wheel)child.wheel.set({steeringAngle:child.wheel.settings.part?steer:0});
  }
  const tour=session.get('coarseTour');if(tour!==null&&tour!==undefined&&this.state.time>(this.props.case==='low'?30:18)){const sequence=['low','medium','offset','high','rotated','group','specimen'];const index=Number(tour)+1;if(index<sequence.length){session.set('coarseTour',index);scenes.load('Scenes/'+sequence[index]+'.judas');}else{session.set('coarseTour',null);this.state.automatic=false;}}
 }
 presentationUpdate(){const car=this.current();if(!car)return;const frame=world.entity('1').presentedTransform,heading=qm(frame.rotation,axis({x:0,y:1,z:0},this.state.yaw)),rotation=qm(heading,axis({x:1,y:0,z:0},this.state.pitch));world.setView({position:add(car.presentedTransform.position,rotate(heading,{x:0,y:3.1,z:7})),rotation},60,{near:.06,far:350});}
 uiUpdate(){if(!this.hud)return;if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}if(input.pressed('save_game'))this.command('save');if(input.pressed('load_game'))this.command('load');if(this.request){const s=saves.status(this.request);if(s&&['completed','failed','cancelled'].includes(s.state)){this.message=s.state+(s.error?' / '+s.error:'');this.request=0;}}
  const car=this.current(),m=car&&this.material(car)?.state;this.hud.get('title').text='JUDAS / PHYSICAL CRASH LAB';this.hud.get('score').text=this.props.case.toUpperCase()+' / '+(this.state.automatic?'WATCH':'MANUAL');this.hud.get('progress').text=car?`Speed ${this.speed(car).toFixed(1)} m/s / failed joints ${m?.interfaces.filter(i=>i.broken).length||0} / permanent crush ${Math.max(0,...(m?.interfaces.map(i=>length(i.plasticTranslation))||[])).toFixed(3)} m / drive ${(car.powertrain?.state.links||[]).filter(i=>i.connected).length}/4`:'';if(this.props.case==='specimen')this.hud.get('progress').text=`Permanent bend ${Math.max(0,...(m?.interfaces.map(i=>length(i.plasticRotation))||[])).toFixed(3)} rad / failed interfaces ${m?.interfaces.filter(i=>i.broken).length||0}`;this.hud.get('message').text=this.message;this.hud.get('controls').text='V watch | T tour | W/S drive | A/D steer | Space brake | 1–8 cases | Tab | P spawn | F3 annex | J collision | F5/F8 save/load | R | Esc';}
 command(op){if(!this.request)this.request=op==='save'?saves.save('physical-crash',{name:'Actual material history, body geometry and driveline'}):saves.load('physical-crash');}
 onUI(e){if(e.type==='back'||e.type==='click'&&e.element==='resume'){this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;}if(e.type==='click'&&e.element==='restart')scenes.reload();if(e.type==='click'&&['save','load'].includes(e.element))this.command(e.element);if(e.type==='click'&&e.element==='quit')ui.quit();}
 destroy(){if(this.region)scenes.releaseRegion(this.region);input.pointerCapture=false;world.clearView();}
}
