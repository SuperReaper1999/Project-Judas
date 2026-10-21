import {world,input,ui,scenes,saves,profiler} from 'judas';

// Human commands only. All balance, step planning and recovery mechanics live
// in Judas. No automatic task requests, physical repairs or scoring driver.
export const properties={mode:{type:'string',default:'standing'},prefab:{type:'string',default:''},
  actorA:{type:'entity',default:null},actorB:{type:'entity',default:null},
  actorC:{type:'entity',default:null},actorD:{type:'entity',default:null},namesJson:{type:'string',default:'{}'}};
const vec=(x=0,y=0,z=0)=>({x,y,z});
const add=(a,b)=>vec(a.x+b.x,a.y+b.y,a.z+b.z);
const axis=(v,t)=>({x:v.x*Math.sin(t/2),y:v.y*Math.sin(t/2),z:v.z*Math.sin(t/2),w:Math.cos(t/2)});
const multiply=(a,b)=>({x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z});
const rotate=(q,v)=>{const r=multiply(multiply(q,{...v,w:0}),{x:-q.x,y:-q.y,z:-q.z,w:q.w});return vec(r.x,r.y,r.z);};
export default class {
  constructor({properties}){this.props=properties;this.state={selected:2,stepping:true,effort:true,pushAngle:0,yaw:0,pitch:-.12,elapsed:0,spawned:[],refusal:''};}
  start(){this.bind();}
  restore(){this.bind();}
  bind(){
    this.actors=[this.props.actorA,this.props.actorB,this.props.actorC,this.props.actorD];
    for(const ref of this.state.spawned){const e=saves.resolve(ref);if(e?.valid)this.actors.push(e);}
    this.names=JSON.parse(this.props.namesJson);this.hud=ui.get('balance_lab');this.hud.modal=false;
    this.hud.get('pause').visible=false;ui.debugOverlayVisible=false;input.pointerCapture=true;this.camera();
  }
  uiUpdate(){
    this.pauseEdge=input.pressed('pause');if(this.pauseEdge)this.pause(!this.hud.modal);
    if(input.pressed('save'))this.receipt=saves.save('balance-lab',{name:'M77 '+this.props.mode});
    if(input.pressed('load'))this.receipt=saves.load('balance-lab');
  }
  pause(value){this.hud.modal=value;this.hud.get('pause').visible=value;input.pointerCapture=!value;}
  onUI(e){if(e.type==='back'&&this.pauseEdge)return;if(e.type==='back'||e.type==='click'&&e.element==='resume')this.pause(false);if(e.type==='click'&&e.element==='reload')scenes.reload();}
  selected(){return this.actors[this.state.selected%this.actors.length];}
  profile(){return this.state.selected===1?'receiver':this.state.selected===2?'stride':'anchor';}
  command(fn){try{fn();this.state.refusal='';}catch(error){this.state.refusal=String(error);}}
  update(){
    if(this.hud.modal)return;
    this.state.yaw-=input.axis('look_x')*.002;this.state.pitch=Math.max(-.8,Math.min(.6,this.state.pitch-input.axis('look_y')*.002));
    for(const name of ['standing','supports','recovery','radial'])if(input.pressed(name))scenes.load('Scenes/'+name+'.judas');
    if(input.pressed('reset'))scenes.reload();
    if(input.pressed('select'))this.state.selected=(this.state.selected+1)%this.actors.length;
    if(input.pressed('push_rotate'))this.state.pushAngle+=Math.PI/4;
    if(input.pressed('push_reverse'))this.state.pushAngle+=Math.PI;
    if(input.pressed('stepping'))this.state.stepping=!this.state.stepping;
    if(input.pressed('effort'))this.state.effort=!this.state.effort;
    const e=this.selected();
    if(input.pressed('diagnostics')&&e?.valid)this.command(()=>e.ragdoll.balanceDiagnostics=!e.ragdoll.balanceDiagnostics);
    if(input.pressed('spawn'))this.command(()=>{const p=e?.presentedTransform.position??vec();const spawned=world.spawnPrefab(this.props.prefab,{position:vec(p.x+2.5,this.props.mode==='radial'?100:0,p.z+3)});if(spawned){this.actors.push(spawned);this.state.spawned.push(saves.reference(spawned));this.state.selected=this.actors.length-1;}});
  }
  fixedUpdate(dt){
    if(this.hud.modal)return;this.state.elapsed+=dt;
    // Ordinary M71 kinematic support motion. This is not actor actuation.
    if(this.props.mode==='supports')for(const i of [0,2]){const support=world.entity(String(20+i));if(support?.valid)support.setKinematicVelocity(vec(.025*Math.cos(this.state.elapsed*.4),0,0),vec(0,.025*Math.cos(this.state.elapsed*.3),0));}
    const e=this.selected();if(!e?.valid||!e.animation?.info.ready)return;
    profiler.scope('M77 human intent',()=>{
      for(const [action,task] of [['stand','stand'],['recover','recover']])if(input.pressed(action))this.command(()=>e.ragdoll.requestBalance(task,{stepping:this.state.stepping,effort:this.state.effort,motorHandoff:true}));
      if(input.pressed('cancel'))this.command(()=>{e.ragdoll.cancelBalance();if(!e.ragdoll.active)e.ragdoll.setMode('passive',{motorHandoff:true});});
      if(input.pressed('weak')||input.pressed('impact')||input.pressed('strong'))this.command(()=>{
        const roots=this.names[this.profile()];
        const root=e.ragdoll.body(roots[0]);if(!root)throw Error('Request physical authority before pushing');
        const magnitude=input.pressed('strong')?100:input.pressed('weak')?2:29;
        const direction=vec(Math.sin(this.state.pushAngle),0,Math.cos(this.state.pushAngle));
        this.state.pushes=(this.state.pushes??0)+1;this.state.impulseBody=root.id;
        root.applyImpulseAtPoint(vec(direction.x*magnitude,direction.y*magnitude,direction.z*magnitude),root.transform.position);this.state.impulseVelocity=root.velocity;
      });
      if(input.pressed('remove'))this.command(()=>{const support=e.ragdoll.balanceState.supports.find(s=>s.entity?.valid)?.entity;if(support)support.destroy();});
      if(input.pressed('return'))this.command(()=>{
        // Separate authorization, derived current owner frame; the native motor
        // validates placement/path and inherits real physical-root velocity.
        const p=e.transform,b=e.ragdoll.balanceState;if(b.status!=='succeeded')throw Error('Motor return requires measured physical standing in this lab');
        const supports=b.supports.filter(s=>s.contacts);const plane=supports.reduce((sum,s)=>sum+s.point.x*b.up.x+s.point.y*b.up.y+s.point.z*b.up.z,0)/supports.length;
        const distance=p.position.x*b.up.x+p.position.y*b.up.y+p.position.z*b.up.z-plane;
        const position=[p.position.x-b.up.x*distance,p.position.y-b.up.y*distance,p.position.z-b.up.z*distance];
        e.ragdoll.setMode('animation',{fade:.5,resumeMotor:true,placement:{position,rotation:[p.rotation.x,p.rotation.y,p.rotation.z,p.rotation.w]}});
      });
      if(input.pressed('layer'))this.command(()=>{if(e.animation.layers.some(l=>l.id==='lab-rest'))e.animation.removeLayer('lab-rest');else e.animation.layer('lab-rest',{clip:'Rest',weight:.5,mask:[this.names[this.profile()][2]]});});
      if(input.pressed('partial'))this.command(()=>{
        if(e.ragdoll.physicalState.mode==='partial')e.ragdoll.setMode('animation',{fade:.3});
        else{if(e.ragdoll.physicalState.mode!=='animation')throw Error('Return to the motor before enabling partial compatibility');e.ragdoll.configurePhysical({enabled:true,regions:[{id:'lab-leg',joints:this.names[this.profile()].slice(1,4),stiffness:30,damping:4,maxTorque:20,effortWeight:1,poseWeight:1}]});e.ragdoll.setMode('partial',{fade:.3});}
      });
      if(e.character&&['animation','partial'].includes(e.ragdoll.physicalState.mode))e.character.velocity=vec(input.axis('move_x')*2,0,-input.axis('move_y')*2);
    });
  }
  camera(){
    const center=add(this.selected()?.presentedTransform.position??vec(0,this.props.mode==='radial'?100:0,0),vec(0,1,0));
    const rotation=multiply(axis(vec(0,1,0),this.state.yaw),axis(vec(1,0,0),this.state.pitch));
    world.setView({position:add(center,rotate(rotation,vec(0,1,6))),rotation},55,{near:.08,far:250});
  }
  presentationUpdate(){
    this.camera();if(!this.hud)return;const e=this.selected();const lines=[`${this.props.mode} | selected ${this.state.selected+1}/${this.actors.length} | steps ${this.state.stepping?'allowed':'denied'} | effort ${this.state.effort?'on':'off'} | push ${(this.state.pushAngle*180/Math.PI).toFixed(0)} deg`];
    if(e?.valid&&e.animation?.info.ready){const b=e.ragdoll.balanceState;lines.push(`${this.profile()} #${e.id} | ${e.ragdoll.physicalState.mode} | ${b.task}/${b.status}${b.pending?' pending '+b.pending:''}`);lines.push(`Actual sample ${b.available?'available':'unavailable'} | height ${b.height.toFixed(3)} m | speed ${b.speed.toFixed(3)} m/s | upright ${b.upright.toFixed(3)} | saturated ${b.saturated}`);lines.push(`Contacts ${b.supports.map(s=>s.contacts).join('/')} | lifted ${b.step.lifted} | landings ${b.step.landings} | elapsed ${b.elapsed.toFixed(2)} s | dwell ${b.settled.toFixed(2)} s`);if(b.diagnostic)lines.push(b.diagnostic);}else lines.push('Normal asynchronous model import pending');
    if(this.receipt){const r=saves.status(this.receipt);lines.push('Save/load '+r?.state+(r?.error?' '+r.error:''));}
    if(this.state.refusal)lines.push(this.state.refusal);this.hud.get('status').text=lines.join('\n');
  }
  destroy(){input.pointerCapture=false;world.clearView();}
}
