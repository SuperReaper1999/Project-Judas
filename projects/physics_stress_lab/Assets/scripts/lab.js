import {world, physics, input, ui, scenes, saves, profiler} from 'judas';
import {add, mul, norm, cross, look} from './math.js';

export const properties = {
 count: {type:'number', default:128},
 tilted: {type:'boolean', default:false}
};
const ASSETS = {box:'248ca2c01ae1684d44885237c41f24d2', sphere:'31cd485353e68fdea03c2473cad25025'};
const v = (x=0,y=0,z=0) => ({x,y,z});
const clamp = (x,a,b) => Math.max(a,Math.min(b,x));

// One shared controller owns lab policy. The hundreds of bodies and native
// constraints have no per-body JavaScript update callback.
export default class {
 constructor({properties}={}) {
  this.props=properties||{};
  this.state={time:0,motors:false,tour:false,tourTime:0,tourStage:-1,extra:[],
   eye:v(28,22,46),yaw:.52,pitch:-.39,selected:0,kicks:0,directions:Array(32).fill(1)};
  this.commands=[];this.hudAge=1;this.request=0;this.notice='';
 }
 start(){this.bind();}
 restore(){this.bind();}
 bind(){
  this.doc=ui.get('physics_stress_lab');this.doc.modal=false;this.doc.get('pause').visible=false;
  ui.debugOverlayVisible=false;input.pointerCapture=true;
  this.drives=world.queryTags(['arcade_drive']).map((owner,index)=>({owner,joint:physics.joint(owner),index,direction:this.state.directions[index]}));
  this.bodies=world.queryTags(['stress_dynamic']);this.chassis=world.queryTags(['arcade_chassis']);
  this.loose=world.queryTags(['loose_piece']);this.commands=[];this.hudAge=1;this.request=0;
  this.extra=this.state.extra.map(reference=>saves.resolve(reference)).filter(entity=>entity?.valid);
  this.setMotors(this.state.motors);
 }
 local(point){
  if(!this.props.tilted)return point;
  const c=Math.cos(.68),s=Math.sin(.68);
  return v(c*point.x-s*point.y,s*point.x+c*point.y,point.z);
 }
 pause(open){this.doc.modal=open;this.doc.get('pause').visible=open;input.pointerCapture=!open;}
 uiUpdate(){if(input.pressed('pause'))this.pause(!this.doc.modal);}
 onUI(event){
  if(event.document!=='physics_stress_lab')return;
  if(event.type==='back'){this.pause(false);return;}
  if(event.type==='click'&&event.element==='resume')this.pause(false);
  if(event.type==='click'&&event.element==='reload')scenes.reload();
 }
 update(dt){
  if(this.doc.modal)return;
  this.state.yaw-=input.axis('look_x')*.002;
  this.state.pitch=clamp(this.state.pitch-input.axis('look_y')*.002,-1.45,1.45);
  const forward=v(-Math.sin(this.state.yaw),0,-Math.cos(this.state.yaw));
  const right=cross(forward,v(0,1,0));
  const travel=add(add(mul(right,input.axis('move_x')),mul(forward,input.axis('move_z'))),v(0,input.axis('move_y'),0));
  this.state.eye=add(this.state.eye,mul(norm(travel),dt*15));
  for(const command of ['motors','kick_one','kick_all','spawn','watch'])if(input.pressed(command))this.commands.push(command);
  for(const [name,path] of [['count_128','room128'],['count_512','room512'],['count_1024','room1024'],['rotated','rotated']])
   if(input.pressed(name))scenes.load('Scenes/'+path+'.judas');
  if(input.pressed('reset'))scenes.reload();
  if(input.pressed('save'))this.request=saves.save('physics-stress-lab',{name:'Mechanical arcade'});
  if(input.pressed('load'))this.request=saves.load('physics-stress-lab');
 }
 setMotors(enabled){
  this.state.motors=enabled;
  for(const drive of this.drives)if(drive.joint?.valid)drive.joint.setMotor(drive.direction*.28,100,enabled);
 }
 kick(all){
  const targets=all?this.chassis:[this.chassis[this.state.selected%this.chassis.length]];
  for(const entity of targets)if(entity?.valid)entity.applyImpulseAtPoint(this.local(v(0,6,11)),add(entity.transform.position,this.local(v(.8,.15,0))));
  this.state.selected++;this.state.kicks+=targets.length;
  this.notice=all?'Physical impulses to all articulated shelves':'Physical impulse to one articulated shelf';
 }
 drop(){
  // New bodies enter above an empty part of the solid pit in a separated grid.
  // A bounded authored cap prevents unattended tours allocating forever.
  if(this.extra.length>=256){this.notice='Runtime drop cap: 256. Reload to reset.';return;}
  const batch=Math.floor(this.extra.length/32);
  for(let index=0;index<32;index++){
   const position=this.local(v(14+(index%8)*.72,4+batch*.75,10+Math.floor(index/8)*.72));
   const entity=world.spawnPrefab(index%2?ASSETS.box:ASSETS.sphere,{position,rotation:this.props.tilted?{w:Math.cos(.34),x:0,y:0,z:Math.sin(.34)}:{w:1,x:0,y:0,z:0}});
   if(entity?.valid){this.extra.push(entity);this.state.extra.push(saves.reference(entity));this.bodies.push(entity);this.loose.push(entity);}
  }
  this.notice='Dropped 32 real rigid bodies';
 }
 fixedUpdate(dt){
  if(this.doc.modal)return;
  this.state.time+=dt;
  for(const command of this.commands.splice(0)){
   if(command==='motors'){this.state.tour=false;this.setMotors(!this.state.motors);}
   if(command==='kick_one')this.kick(false);
   if(command==='kick_all')this.kick(true);
   if(command==='spawn')this.drop();
   if(command==='watch'){this.state.tour=!this.state.tour;this.state.tourTime=0;this.state.tourStage=-1;}
  }
  if(this.state.tour){
   this.state.tourTime=(this.state.tourTime+dt)%36;
   const phase=this.state.tourTime<8?0:this.state.tourTime<20?1:2;
   if(phase!==this.state.tourStage){
    this.state.tourStage=phase;this.setMotors(phase===0);
    if(phase===2)this.kick(false);
    this.notice=['Finite motors drive all 32 shelves','Motor power off: springs settle and ordinary islands sleep','An impulse wakes a connected assembly'][phase];
   }
  }
  if(this.state.motors)for(const drive of this.drives)if(drive.joint?.valid){
   const coordinate=drive.joint.state.coordinate;
   if((coordinate>.225&&drive.direction>0)||(coordinate<-.225&&drive.direction<0)){
    drive.direction*=-1;this.state.directions[drive.index]=drive.direction;drive.joint.setMotor(drive.direction*.28,100,true);
   }
  }
  this.hudAge+=dt;
  if(this.hudAge>.4){this.hudAge=0;this.refreshHud();}
 }
 refreshHud(){
  let asleep=0,alive=0;
  for(const entity of this.bodies)if(entity?.valid){alive++;if(entity.sleeping)asleep++;}
  profiler.counter('Stress Lab dynamic bodies',alive,'latest');
  profiler.counter('Stress Lab sleeping bodies',asleep,'latest');
  profiler.counter('Stress Lab arcade mechanisms',this.drives.length,'latest');
  let text=`32 ARCADE MECHANISMS / 96 ARTICULATED BODIES + 32 FREE BALLS + ${this.loose.length} LOOSE PIECES\n`;
  text+=`${alive} dynamic / ${asleep} naturally sleeping / ${alive-asleep} awake   |   finite motors ${this.state.motors?'ON':'OFF'}${this.state.tour?' / WATCH TOUR':''}\n`;
  text+=`${this.props.tilted?'ROTATED GRAVITY ROOM':'FLAT GRAVITY ROOM'}   |   WASD move / Q down / E up / mouse look   |   F6 save / F7 load / R reload\n`;
  text+=this.notice||'No scripted freezing, distance culling or decorative replacement of physical bodies.';
  if(this.request){const status=saves.status(this.request);text+=`\nSave/load: ${status?.state||'pending'} ${status?.error||''}`;}
  this.doc.get('status').text=text;
 }
 presentationUpdate(){
  let eye=this.state.eye,forward=v(-Math.sin(this.state.yaw)*Math.cos(this.state.pitch),Math.sin(this.state.pitch),-Math.cos(this.state.yaw)*Math.cos(this.state.pitch));
  if(this.state.tour){
   const t=this.state.tourTime*.11;
   eye=v(25*Math.sin(t),15+3*Math.sin(t*.7),29+12*Math.cos(t));
   forward=norm(add(v(0,1,-9),mul(eye,-1)));
  }
  world.setView({position:this.local(eye),rotation:look(this.local(forward),this.local(v(0,1,0)))},65,{near:.08,far:350});
 }
 destroy(){input.pointerCapture=false;world.clearView();}
}
