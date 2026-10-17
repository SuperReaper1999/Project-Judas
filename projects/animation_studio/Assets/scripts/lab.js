import {world,input,ui,scenes,saves,physics} from 'judas';
import {vec,add,scale,axis,multiply,rotate} from './math.js';
export const properties={rotated:{type:'boolean',default:false}};
export default class Studio {
 constructor({properties}){this.rotated=properties.rotated;this.state={time:0,clip:'Idle',action:'',footsteps:0,strikes:0,interactions:0,tour:false,last:'Ready',yaw:0};}
 start(){this.bind();}
 restore(){this.bind();}
 bind(){this.saveRequest=null;this.state.requestState='';this.state.requestError='';this.actor=world.entity('10');this.target=world.entity('30');this.beacon=world.entity('31');this.doc=ui.get('studio');this.doc.modal=false;this.doc.get('pause').visible=false;ui.debugOverlayVisible=false;input.pointerCapture=false;this.basis=this.rotated?axis(vec(0,0,1),Math.PI/2):{x:0,y:0,z:0,w:1};}
 uiUpdate(){this.opened=false;if(input.pressed('pause')&&!this.doc.modal){this.pause(true);this.opened=true;}if(input.pressed('save'))this.saveRequest=saves.save('motion-studio',{name:'Motion Studio'});if(input.pressed('load'))this.saveRequest=saves.load('motion-studio');}
 pause(on){this.doc.modal=on;this.doc.get('pause').visible=on;}
 onUI(e){if(e.document!=='studio')return;if((e.type==='back'&&!this.opened)||(e.type==='click'&&e.element==='resume'))this.pause(false);if(e.type==='click'&&e.element==='reload')scenes.reload();}
 update(){const request=this.saveRequest?saves.status(this.saveRequest):null;if(request){this.state.requestState=request.state;this.state.requestError=request.error;}if(input.pressed('reset'))scenes.reload();if(input.pressed('rotate'))scenes.load('Scenes/rotated.judas');if(input.pressed('flat'))scenes.load('Scenes/studio.judas');if(input.pressed('tour'))this.state.tour=!this.state.tour;
  this.doc.get('status').text=`${this.state.clip.toUpperCase()} / ${this.rotated?'ROTATED GRAVITY':'FLAT GRAVITY'} / ${this.state.tour?'WATCH TOUR':'MANUAL'}\nFootstep markers ${this.state.footsteps} | physical strikes ${this.state.strikes} | interactions ${this.state.interactions}\n${this.state.last}${this.state.requestState?" / save: "+this.state.requestState:""}${this.state.requestError?" / "+this.state.requestError:""}\nEditor: View > Animation timeline > performer.judasimport > Load / reimport.`;
 }
 choose(name){if(name===this.state.clip)return;this.actor.animation.crossFade(name,.18);this.actor.animation.loop=['Walk','Run','Idle'].includes(name);this.state.clip=name;}
 fixedUpdate(dt){if(!this.actor?.valid||!this.actor.animation?.info.ready)return;this.state.time+=dt;
  let x=(input.held('right')?1:0)-(input.held('left')?1:0),z=(input.held('back')?1:0)-(input.held('forward')?1:0),run=input.held('run');
  const tour=this.state.tour,phase=this.state.time%12;
  if(tour){x=0;z=phase<1.7?-1:phase>7&&phase<8.7?1:0;run=false;}
  const m=this.actor.character,up=rotate(this.basis,vec(0,1,0));let velocity=m.velocity;
  if(m.supported)velocity=m.supportVelocity;
  if((input.pressed('jump')||(tour&&phase>9&&phase<9+dt*1.2))&&m.supported){velocity=add(add(velocity,this.state.lastRoot??vec()),scale(up,4.6));this.state.action='Jump';this.state.actionUntil=this.state.time+.85;}
  if(input.pressed('strike')||(tour&&phase>2&&phase<2+dt*1.2)){this.state.action='Strike';this.state.actionUntil=this.state.time+.8;}
  if(input.pressed('interact')||(tour&&phase>5&&phase<5+dt*1.2)){this.state.action='Interact';this.state.actionUntil=this.state.time+1.1;}
  if(this.state.time>this.state.actionUntil)this.state.action='';
  if(x||z)this.state.yaw=Math.atan2(-x,-z);
  const orientation=multiply(this.basis,axis(vec(0,1,0),this.state.yaw));m.orient(orientation);m.velocity=velocity;
  this.choose(this.state.action||(x||z?(run?'Run':'Walk'):'Idle'));
  // This single fixed-step advance owns the animation clock until play/resume.
  // Extracted model-local motion is transformed once into WORLD space.
  const sample=this.actor.animation.advance(dt);
  const rootDelta=rotate(orientation,sample.motion.translation);
  this.state.lastRoot=(x||z)?scale(rootDelta,1/dt):vec();
  if(x||z)m.displace(rootDelta);
  if(sample.overflow)throw Error('Authored marker density exceeds bounded sample');
  for(const event of sample.events){
   // Blends expose all contributors; project policy chooses a dominant event.
   if(event.reverse||event.source!=='base'||event.weight<.5)continue;
   this.state.last=`${event.clip} / ${event.name} @ ${event.time.toFixed(3)} s`;
   if(event.name==='footstep')this.state.footsteps++;
   if(event.name==='interact'){this.state.interactions++;this.beacon.material().set({baseColor:this.state.interactions%2?{x:.95,y:.67,z:.2,a:1}:{x:.12,y:.75,z:.65,a:1}});}
   if(event.name==='contact'){
    const origin=add(this.actor.transform.position,rotate(orientation,vec(0,1,-.35))),direction=rotate(orientation,vec(0,0,-1));
    const hit=physics.raycast(origin,direction,2.1,{ignored:[this.actor]});
    if(hit?.entity?.valid&&hit.entity.id===this.target.id){hit.entity.applyImpulseAtPoint(add(scale(direction,13),scale(up,2)),hit.point);this.state.strikes++;}
   }
  }
 }
 presentationUpdate(){if(!this.actor?.valid)return;const p=this.actor.presentedTransform.position;const offset=rotate(this.basis,vec(0,3.4,6));world.setView({position:add(p,offset),rotation:multiply(this.basis,axis(vec(1,0,0),-.32))},55,{near:.1,far:100});}
 destroy(){world.clearView();}
}
