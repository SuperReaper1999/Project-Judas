import {world,input,ui,scenes,saves,profiler} from 'judas';
import {vec,add,scale,arr,quat,qa,axis,multiply,rotate,transformPoint,mixPose} from './math.js';
import {mapping,physicalSettings,contact} from './mapping.js';

export const properties={mode:{type:'string',default:'wall'},prefab:{type:'string',default:''},actorA:{type:'entity',default:null},actorB:{type:'entity',default:null},namesJson:{type:'string',default:'{}'}};

// The lab selects demonstration targets/modes. Native Judas solves the poses,
// drives ordinary physics joints and owns all final bone/body results.
export default class {
  constructor({properties}){
    this.props={...properties,names:JSON.parse(properties.namesJson)};this.state={elapsed:0,variant:false,impossible:false,carry:false,hidden:false,clip:false,effort:1,yaw:0,pitch:-.06,spawned:[],initialized:[],boards:{},refusal:''};
  }
  start(){this.bind(false);}
  restore(){this.bind(true);}
  bind(restored){
    this.hud=ui.get('character_lab');this.hud.modal=false;this.hud.get('pause').visible=false;ui.debugOverlayVisible=false;input.pointerCapture=true;
    this.actors=[this.props.actorA,this.props.actorB];
    for(const ref of this.state.spawned){const e=saves.resolve(ref);if(e)this.actors.push(e);}
    this.restored=restored;this.pauseEdge=false;this.camera();
  }
  uiUpdate(){
    this.pauseEdge=input.pressed('pause');if(this.pauseEdge){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}
    if(input.pressed('save'))this.saveRequest=saves.save('character-lab',{name:'M70 '+this.props.mode});
    if(input.pressed('load'))this.saveRequest=saves.load('character-lab');
  }
  onUI(e){
    if(e.type==='back'&&this.pauseEdge)return;
    if(e.type==='back'||e.type==='click'&&e.element==='resume'){this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;}
    if(e.type==='click'&&e.element==='reload')scenes.reload();
  }
  update(dt){
    if(this.hud.modal)return;
    this.state.yaw-=input.axis('look_x')*.002;this.state.pitch=Math.max(-1,Math.min(.75,this.state.pitch-input.axis('look_y')*.002));
    for(const [action,scene] of [['wall','wall'],['board','board'],['physical','physical']])if(input.pressed(action))scenes.load('Scenes/'+scene+'.judas');
    if(input.pressed('reset'))scenes.reload();
    if(input.pressed('adjust'))this.state.variant=!this.state.variant;
    if(input.pressed('impossible'))this.state.impossible=!this.state.impossible;
    if(input.pressed('clips')){this.state.clip=!this.state.clip;for(const e of this.actors)if(e.animation?.info.ready)e.animation.crossFade(this.state.clip?'Wave':'Idle',.45);}
    if(input.pressed('layer')){this.state.carry=!this.state.carry;for(let i=0;i<this.actors.length;i++){const e=this.actors[i],n=this.names(i);if(e.animation?.info.ready)e.animation.layer('carry',{clip:'Carry',mask:[n.ShoulderA,n.ElbowA,n.PalmA,n.ShoulderB,n.ElbowB,n.PalmB],weight:.7,enabled:this.state.carry});}}
    if(input.pressed('head')){this.state.hidden=!this.state.hidden;for(let i=0;i<this.actors.length;i++){const e=this.actors[i];if(e.animation?.info.ready)e.animation.layer('head',{clip:'HeadHidden',mask:[this.names(i).Cranial],weight:1,enabled:this.state.hidden});}}
    if(this.props.mode==='physical'&&input.pressed('spawn')){const e=world.spawnPrefab(this.props.prefab,{position:{x:0,y:0,z:2.5}});if(e){this.actors.push(e);const ref=saves.reference(e);if(ref)this.state.spawned.push(ref);}}
    // Presentation never provides drive history or authoritative target poses.
  }
  names(index){return this.props.names[index===1?'alternate':'figure'];}
  fixedUpdate(dt){
    if(this.hud.modal)return;
    this.state.elapsed+=dt;
    const driveEdge=input.pressed('drive');if(driveEdge)this.state.effort=this.state.effort?0:1;
    profiler.scope('M70 lab target policy',()=>{
      for(let i=0;i<this.actors.length;i++){
        const e=this.actors[i];if(!e?.valid||!e.animation?.info.ready)continue;
        const n=this.names(i),a=e.animation;
        if(!this.state.initialized.includes(e.id)){
          // Fresh restore already owns durable native targets/modes: don't replay
          // activation or spawn defaults after loading an active articulation.
          if(!this.restored){a.configureIK(this.props.mode==='physical'?null:mapping(n));e.ragdoll.configurePhysical(physicalSettings(n,this.state.effort));if(this.props.mode==='physical')e.ragdoll.setMode('partial',{fade:.25});}
          this.state.initialized.push(e.id);
        }
        if(this.props.mode!=='physical'){
          a.ikTargets(this.targets(e,i));
          if(e.character&&['animation','partial'].includes(e.ragdoll.physicalState.mode))e.character.velocity=vec();
        }else if(i===0&&e.character&&['animation','partial'].includes(e.ragdoll.physicalState.mode)){
          // Ordinary JS locomotion; CharacterMotor has no animation/ragdoll rules.
          const m=e.character,s=m.state;let velocity=m.velocity;
          if(s.supported){velocity=add(s.supportVelocity,vec(input.axis('move_x')*2.5,0,-input.axis('move_y')*2.5));if(input.pressed('jump'))velocity=add(velocity,scale(m.up,5));}
          m.velocity=velocity;
        }
        if(input.pressed('impact')){const b=e.ragdoll.body(n.ElbowA);if(b)b.applyImpulseAtPoint({x:3,y:.4,z:-2},b.transform.position);}
        if(driveEdge)e.ragdoll.configurePhysical(physicalSettings(n,this.state.effort));
        if(input.pressed('active'))this.request(e,'active',{fade:.25,motorHandoff:true});
        if(input.pressed('passive'))this.request(e,'passive',{fade:.25,motorHandoff:true});
        if(input.pressed('animated')){
          // The game explicitly chooses placement. Native collision validation
          // can refuse it; no auto get-up, root relocation or wall penetration.
          const p=e.transform.position;this.request(e,'animation',{fade:.5,resumeMotor:true,placement:{position:[p.x,1.1,p.z],rotation:[0,0,0,1]}});
        }
      }
    });
    profiler.counter('M70 lab instances',this.actors.length,'latest');
  }
  request(entity,mode,options){
    // An explicit user placement can be refused by normal collision validation.
    // Preserve physical authority and keep the controls alive to try elsewhere.
    try{entity.ragdoll.setMode(mode,options);this.state.refusal='';}catch(error){this.state.refusal=String(error);}
  }
  targets(e,index){
    const t=e.transform;const alternate=index===1,w=alternate?1.18:1,leg=alternate?.92:1;
    if(this.props.mode==='wall'){
      const reach=this.state.variant?.04:0,far=this.state.impossible?1.5:0;
      const values=[['palmA',vec(-.39*w,1.66+far,-.26-reach),[0,-.055,0]],['palmB',vec(.39*w,1.66,-.26-reach),[0,-.055,0]],['soleA',vec(-.15*w,.22,-.34),[0,-.04,0]],['soleB',vec(.15*w,.22,-.34),[0,-.04,0]]];
      return values.map(([id,p,offset])=>contact(id,id,arr(transformPoint(t,p)),qa(multiply(t.rotation,id.startsWith('sole')?axis(vec(1,0,0),Math.PI/2):quat())),offset));
    }
    const board=world.entity(index===1?'21':'20');const center=vec(t.position.x,.025,t.position.z);
    const angle=(this.state.variant?.16:.1)*Math.sin(this.state.elapsed*.7),yaw=.12*Math.sin(this.state.elapsed*.4);
    const rotation=multiply(axis(vec(0,1,0),yaw),axis(vec(0,0,1),angle));
    const position=add(center,vec(.04*Math.sin(this.state.elapsed*.5),0,.035*Math.sin(this.state.elapsed*.4)));
    if(board)board.transform={position,rotation};
    const current={position,rotation},old=this.state.boards[index];this.state.boards[index]={previous:old?.current??current,current};
    const frame={position,rotation,scale:vec(1,1,1)};
    return ['A','B'].map((side,k)=>{const p=vec((k?1:-1)*.15*w,.04,-.1);if(this.state.variant&&k===1)p.y+=.08;return contact('sole'+side,'sole'+side,arr(transformPoint(frame,p)),qa(rotation),[0,-.04,-.1]);});
  }
  camera(){
    const center=this.props.mode==='physical'&&this.actors?.[0]?.valid?add(this.actors[0].presentedTransform.position,vec(1.6,1.2,0)):vec(0,1.1,0);
    const rotation=multiply(axis(vec(0,1,0),this.state.yaw),axis(vec(1,0,0),this.state.pitch));
    world.setView({position:add(center,rotate(rotation,vec(0,.25,6))),rotation},55,{near:.08,far:150});
  }
  presentationUpdate(_dt,alpha){
    // The collider remains at the actual fixed transform. A body-free visual
    // clone samples that known history at the same alpha as the resolved bones.
    if(this.props.mode==='board')for(let i=0;i<2;i++){const frame=this.state.boards[i],visual=world.entity(String(22+i));if(frame&&visual)visual.transform=mixPose(frame.previous,frame.current,alpha);}
    this.camera();if(!this.hud)return;
    const lines=[`${this.props.mode.toUpperCase()} | impossible ${this.state.impossible} | carry ${this.state.carry} | head hidden ${this.state.hidden} | drive effort ${this.state.effort}`];
    for(let i=0;i<Math.min(2,this.actors.length);i++){
      const e=this.actors[i];if(!e?.valid||!e.animation?.info.ready){lines.push('Async multipart model pending');continue;}
      const s=e.animation.ikStatus,p=e.ragdoll.physicalState;
      lines.push(`#${i+1} root ${e.transform.position.x.toFixed(2)},${e.transform.position.y.toFixed(2)},${e.transform.position.z.toFixed(2)} | ${p.mode}${p.pending?' pending':''} | IK ${s.converged?'reached':'bounded'} ${s.iterations||0} iterations`);
      let worstAngle=0,peakTorque=0,saturated=0,worstJoint=p.drives[0]?.joint??'none';
      for(const d of p.drives){if(Math.abs(d.angleError)>worstAngle){worstAngle=Math.abs(d.angleError);worstJoint=d.joint;}peakTorque=Math.max(peakTorque,Math.abs(d.torque));if(d.saturated)saturated++;}
      lines.push(`  Drive ${(worstAngle*180/Math.PI).toFixed(2)} deg @ ${worstJoint} | peak ${peakTorque.toFixed(2)} N m | saturated ${saturated}/${p.drives.length}`);
      for(const r of s.targets||[])lines.push(`  ${r.id}: ${(r.positionError*100).toFixed(2)} cm / ${(r.orientationError*180/Math.PI).toFixed(2)} deg (${r.status})`);
      if(p.diagnostic)lines.push(p.diagnostic);
    }
    if(this.saveRequest){const s=saves.status(this.saveRequest);lines.push('Save/load '+s?.state+(s?.error?' '+s.error:''));}
    if(this.state.refusal)lines.push(this.state.refusal);
    this.hud.get('status').text=lines.join('\n');
  }
  destroy(){input.pointerCapture=false;world.clearView();}
}
