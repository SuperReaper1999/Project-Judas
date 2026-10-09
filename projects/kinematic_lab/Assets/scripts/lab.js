import {world,input,ui,scenes,saves} from 'judas';

// Original CC0 lab policy. Judas owns every collider, contact impulse and
// motor result. This script chooses trajectories and ordinary player intent.
const v=(x=0,y=0,z=0)=>({x,y,z});
const add=(a,b)=>v(a.x+b.x,a.y+b.y,a.z+b.z);
const mul=(a,s)=>v(a.x*s,a.y*s,a.z*s);
const axis=(a,t)=>({w:Math.cos(t/2),x:a.x*Math.sin(t/2),y:a.y*Math.sin(t/2),z:a.z*Math.sin(t/2)});
const qm=(a,b)=>({w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w});
const rotate=(q,a)=>{const r=qm(qm(q,{w:0,...a}),{w:q.w,x:-q.x,y:-q.y,z:-q.z});return v(r.x,r.y,r.z);};
const identity={w:1,x:0,y:0,z:0};
const rotated=axis(v(0,0,1),-Math.PI/2);
const text=a=>[a.x,a.y,a.z].map(x=>x.toFixed(3)).join(', ');

export default class {
  constructor(){this.state={time:0,stopped:false,direction:1,spin:false,rider:'40',contacts:0,lastContact:'none',refusal:'',removed:false};}
  start(){this.bind();}
  restore(){this.bind();}
  bind(){this.hud=ui.get('kinematic_lab');if(this.hud){this.hud.modal=false;this.hud.get('pause').visible=false;}ui.debugOverlayVisible=false;input.pointerCapture=false;this.camera();}
  camera(){world.setView({position:v(5,11,19),rotation:axis(v(1,0,0),-.43)},65);}
  uiUpdate(){
    if(input.pressed('pause')&&this.hud){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;}
    if(input.pressed('save'))this.saveRequest=saves.save('kinematic-lab',{name:'Kinematic Lab'});
    if(input.pressed('load'))this.saveRequest=saves.load('kinematic-lab');
  }
  onUI(e){if((e.type==='back'||e.type==='click'&&e.element==='resume')&&this.hud){this.hud.modal=false;this.hud.get('pause').visible=false;}if(e.type==='click'&&e.element==='reload')scenes.reload();}
  update(){if(input.pressed('reset'))scenes.reload();}
  fixedUpdate(dt){
    if(this.hud?.modal)return;
    this.state.refusal='';
    const pusher=world.entity('10'),translate=world.entity('20'),spin=world.entity('30'),wall=world.entity('50');
    if(input.pressed('rider_one'))this.state.rider='40';
    if(input.pressed('rider_two'))this.state.rider='41';
    if(input.pressed('rider_rotated'))this.state.rider='42';
    if(input.pressed('reverse'))this.state.direction*=-1;
    if(input.pressed('spin'))this.state.spin=!this.state.spin;
    if(input.pressed('stop')){
      this.state.stopped=!this.state.stopped;
      if(this.state.stopped)for(const e of [pusher,translate,spin,wall])if(e.valid&&e.motionType==='kinematic')e.stopKinematic();
    }
    if(!this.state.stopped)this.state.time+=dt;
    if(input.pressed('dynamic')||input.pressed('kinematic')){
      try{translate.setMotionType(input.pressed('dynamic')?'dynamic':'kinematic');}catch(error){this.state.refusal=String(error);}
    }
    if(input.pressed('remove')&&pusher.valid){pusher.destroy();this.state.removed=true;}
    let fast=false;
    if(input.pressed('fast')&&pusher.valid&&pusher.motionType==='kinematic'){
      // Placement and continuous target are deliberately separate public calls.
      // Reload first to put the same original dynamic crate back in the lane.
      pusher.transform={position:v(-6,.35,2),rotation:identity,scale:v(1,1,1)};
      pusher.moveKinematic({position:v(-1,.35,2),rotation:identity});fast=true;
    }
    if(!this.state.stopped){
      if(pusher.valid&&pusher.motionType==='kinematic'&&!fast){
        const at=pusher.transform.position;if(at.x>-2)this.state.direction=-1;if(at.x<-6)this.state.direction=1;
        pusher.setKinematicVelocity(v(this.state.direction*.65,0,0),v(0,this.state.spin?.7:0,0));
      }
      const t=this.state.time;
      // Replacing these bounded targets each interval smooths policy catch-up
      // after a dropped dynamic platform resumes prescribed authority.
      if(translate.motionType==='kinematic')translate.moveKinematic({position:v(-1+Math.sin(t*.45)*1.5,.75+Math.sin(t*.7)*.35,-5),rotation:identity},.35);
      if(spin.motionType==='kinematic')spin.setKinematicVelocity(v(),v(0,.4*this.state.direction,0));
      if(wall.motionType==='kinematic')wall.moveKinematic({position:v(13+Math.sin(t*.7)*.35,1.5+Math.sin(t*.45)*1.5,-4),rotation:rotated},.35);
    }
    for(const id of ['40','41','42']){
      const rider=world.entity(id),m=rider.character;if(!m)continue;const s=m.state;
      if(s.supported){let intent=v();if(this.state.rider===id){intent=v(input.axis('move_x')*2,0,-input.axis('move_y')*2);if(id==='42')intent=rotate(rotated,intent);}
        let velocity=add(s.supportVelocity,intent);
        if(this.state.rider===id&&input.pressed('jump'))velocity=add(velocity,mul(m.up,4));m.velocity=velocity;
      }
    }
    // Save plain readback for the inspector/VM proof; no native handle is kept.
    if(pusher.valid){this.state.pusherActual=pusher.transform;this.state.pusherVelocity=pusher.velocity;this.state.command=pusher.kinematicMotion;}
  }
  presentationUpdate(){
    // Presentation only reads fixed-step state and positions the camera.
    this.camera();if(!this.hud)return;
    const support=world.entity('20'),pusher=world.entity('10'),rider=world.entity(this.state.rider),m=rider.character,s=m?.state;
    const contacts=pusher.valid?pusher.scriptState(1):null;
    const lines=[`Selected rider ${this.state.rider} / supported ${s?.supported} / support ${s?.supportEntity?.id??'none'}`,
      `Support velocity ${s?text(s.supportVelocity):'none'} / rider velocity ${m?text(m.velocity):'none'}`,
      `Pusher ${pusher.valid?pusher.motionType:'removed'} / actual ${pusher.valid?text(pusher.transform.position):'none'} / COM velocity ${pusher.valid?text(pusher.velocity):'none'}`,
      `Lift authority ${support.motionType} / stopped ${this.state.stopped} / reverse ${this.state.direction} / contacts enter ${contacts?.enter??0}, stay ${contacts?.stay??0}`];
    if(this.state.refusal)lines.push(this.state.refusal);if(this.saveRequest){const request=saves.status(this.saveRequest);lines.push(`Save/load ${request?.state??'pending'} ${request?.error??''}`);}
    this.hud.get('status').text=lines.join('\n');
  }
  destroy(){input.pointerCapture=false;world.clearView();}
}
