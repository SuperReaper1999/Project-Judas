// Game policy only: the starter controller and native MK2 own physical movement.
import Motion from './motion.js';
import {input,scenes,saves,ui} from 'judas';
export const properties={sideView:{type:'boolean',default:false},speed:{type:'number',default:4},
 acceleration:{type:'number',default:35},jumpSpeed:{type:'number',default:6},
 gate:{type:'entity',default:null}};
export default class extends Motion {
 constructor(context){
  super(context);
  this.state={health:3,cells:[],danger:0,nearBeacon:false,charged:false,won:false,lost:false,
   cooldown:0,time:0,pulses:0,request:null,message:'Find the three lantern cells.'};
 }
 fixedUpdate(dt){
  if(this.state.won||this.state.lost){
   this.entity.applyForce({x:-this.entity.velocity.x*this.entity.mass/dt,
    y:-this.entity.velocity.y*this.entity.mass/dt,z:0});return;
  }
  super.fixedUpdate(dt);
  this.state.time+=dt;this.state.cooldown=Math.max(0,this.state.cooldown-dt);
  if(this.state.danger&&this.state.cooldown===0){
   this.state.health--;this.state.cooldown=1;
   this.state.message='The brambles hurt! Get back onto the path.';
   if(this.state.health<=0){this.state.lost=true;this.state.message='The grove claimed you. Press R to retry.';}
  }
  const a=this.entity.spriteAnimation;
  if(!a||a.error)throw Error(a?.error??'Missing traveller animation');
  if(!a.ready)return;
  for(const event of a.events)if(event.name==='strike'){
   this.state.pulses++;
   // The marker, proximity and collected resources are all required.
   if(this.state.nearBeacon&&this.state.cells.length===3&&!this.state.charged){
    if(this.properties.gate)this.properties.gate.destroy();
    this.state.charged=true;this.state.message='Beacon restored! The east gate is open.';
   }
  }
  if(input.pressed('interact')){
   this.state.message=this.state.cells.length<3?'You need all three lantern cells.':
    this.state.nearBeacon?'Restoring the beacon...':'Stand beside the beacon to restore it.';
   if(a.clip!=='interact'||a.finished)this.entity.setSpriteAnimation({clip:'interact',time:0,playing:true,loop:false});
   return;
  }
  if(a.clip==='interact'&&!a.finished)return;
  const clip=Math.hypot(input.axis('move_x'),input.axis('move_y'))>.05?'run':'idle';
  if(a.clip!==clip||!a.playing)this.entity.setSpriteAnimation({clip,time:0,playing:true,loop:true});
 }
 onTriggerEnter(event){
  const other=event.other;if(!other?.valid)return;
  if(other.hasTag('lantern-cell')&&!this.state.cells.includes(other.id)){
   this.state.cells.push(other.id);other.destroy();
   this.state.message=this.state.cells.length===3?'All cells found. Restore the east beacon with E.':'Lantern cell recovered.';
   return; // destroy invalidates the wrapper immediately; don't query it again.
  }
  if(other.hasTag('danger'))this.state.danger++;
  if(other.hasTag('beacon'))this.state.nearBeacon=true;
  if(other.hasTag('exit')&&this.state.charged){this.state.won=true;this.state.message='GROVE SAVED! Press R for another run.';}
 }
 onTriggerExit(event){
  if(!event.other?.valid)return;
  if(event.other?.hasTag('danger'))this.state.danger=Math.max(0,this.state.danger-1);
  if(event.other?.hasTag('beacon'))this.state.nearBeacon=false;
 }
 uiUpdate(){
  if(!saves.busy){
   if(input.pressed('restart'))scenes.reload();
   else if(input.pressed('save_game'))this.state.request=saves.save('grove');
   else if(input.pressed('load_game'))this.state.request=saves.load('grove');
  }
  const hud=ui.get('sprite_help');if(!hud)return;
  const request=this.state.request?saves.status(this.state.request):null;
  hud.get('help').text='BEACON GROVE  |  Hearts '+Math.max(0,this.state.health)+'/3  |  Cells '+this.state.cells.length+'/3\n'+
   this.state.message+'\nWASD / arrows: move   E: restore beacon   K/L: save/load (desktop also F6/F7)   R: retry   Esc: pause'+
   (request?'\nSave: '+request.state+(request.error?' — '+request.error:''):'');
 }
}
