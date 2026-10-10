import {world,input,ui,scenes,saves,profiler} from 'judas';

export const properties={keysJson:{type:'string',default:'{}'},prefab:{type:'string',default:''}};
// This project only plays ordinary baked clips. No source rig/profile or native
// retarget dispatcher is present in gameplay; the target's immutable asset owns them.
export default class {
  constructor({properties}){
    this.props=properties;
    this.keys=JSON.parse(properties.keysJson);
    this.state={clip:'Wave',root:0,layer:false,ik:false,physical:false,paused:false,region:false,spawns:[]};
  }
  start(){this.bind();}
  restore(){this.bind();}
  bind(){
    this.hud=ui.get('retarget_lab');this.hud.modal=false;this.hud.get('pause').visible=false;
    ui.debugOverlayVisible=false;input.pointerCapture=false;
    const source=world.entity('10');this.source=source.valid?source:null;this.targets=[world.entity('11'),world.entity('12'),world.entity('13')];
    this.camera();
  }
  camera(){world.setView({position:{x:1.1,y:2.1,z:9.5},rotation:{x:0,y:0,z:0,w:1}},50,{near:.08,far:120});}
  uiUpdate(){
    this.pauseEdge=input.pressed('pause');
    if(this.pauseEdge){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;}
    if(input.pressed('save'))this.request=saves.save('retarget-lab',{name:'Borrowed ordinary clips'});
    if(input.pressed('load'))this.request=saves.load('retarget-lab');
  }
  onUI(e){
    if(e.type==='back'&&this.pauseEdge)return;
    if(e.type==='back'||e.type==='click'&&e.element==='resume'){this.hud.modal=false;this.hud.get('pause').visible=false;}
    if(e.type==='click'&&e.element==='reload')scenes.reload();
  }
  update(){
    if(this.hud.modal||!this.targets[0]?.animation.info.ready)return;
    const current=[['travel','Travel'],['wave','Wave'],['bob','Bob'],['native','Native']].find(([action])=>input.pressed(action));
    if(current){this.state.clip=current[1];this.play(false);}
    if(input.pressed('crossfade')){this.state.clip=this.state.clip==='Native'?'Wave':'Native';this.play(true);}
    if(input.pressed('root')){this.state.root=(this.state.root+1)%3;this.state.clip='Travel';this.play(false);}
    if(input.pressed('clock')){this.state.paused=!this.state.paused;for(const e of [this.source,...this.targets])if(e?.animation.info.ready)this.state.paused?e.animation.pause():e.animation.resume();}
    if(input.pressed('seek'))for(const e of [this.source,...this.targets])if(e?.animation.info.ready)e.animation.seek((e.animation.time+.25)%2);
    if(input.pressed('layer')){
      this.state.layer=!this.state.layer;
      const k=this.keys.broad;this.targets[0].animation.layer('left-arm',{clip:'Wave',mask:[k.ShoulderA,k.ElbowA,k.PalmA],weight:.7,enabled:this.state.layer});
    }
    if(input.pressed('ik')){
      this.state.ik=!this.state.ik;const a=this.targets[0].animation,k=this.keys.broad;
      a.configureIK(this.state.ik?{bodyRoot:k.Pivot,spine:[k.Vertebra0,k.Vertebra1],rootRotation:false,rootMin:[0,0,0],rootMax:[0,0,0],chains:[{id:'left',joints:[k.ShoulderA,k.ElbowA,k.PalmA]}],iterations:32,positionTolerance:.01,targets:[]}:null);
    }
    if(input.pressed('physical')){
      const e=this.targets[0],k=this.keys.broad;this.state.physical=!this.state.physical;
      if(this.state.physical){e.ragdoll.configurePhysical({enabled:true,regions:[{id:'left',joints:[k.Vertebra1,k.ElbowA,k.PalmA],stiffness:24,damping:4,maxTorque:12,poseWeight:1,effortWeight:1}]});e.ragdoll.setMode('partial',{fade:.25});}
      else e.ragdoll.setMode('animation',{fade:.4});
    }
    if(input.pressed('region')){
      this.state.region=!this.state.region;
      if(this.state.region)this.region=scenes.requestRegion('rig');
      else {if(this.region)scenes.releaseRegion(this.region);this.region=null;scenes.unloadRegion('rig');}
    }
    if(input.pressed('reset'))scenes.reload();
  }
  play(fade){
    const name=this.state.clip==='Travel'?['TravelPreserve','TravelExtract','TravelInPlace'][this.state.root]:this.state.clip;
    for(const e of this.targets.slice(0,2))if(e?.animation.info.ready)fade?e.animation.crossFade(name,.6):e.animation.play(name);
    if(this.source?.animation.info.ready&&this.state.clip!=='Native')this.source.animation.play(this.state.clip);
    // The fourth instance intentionally keeps Wave and a different clock.
    this.targets[2].animation.speed=.7;
  }
  fixedUpdate(){
    if(this.hud.modal)return;
    if(this.state.ik){
      const e=this.targets[0],t=e.animation.jointTransform(this.keys.broad.PalmA,'world');
      if(t)e.animation.ikTargets([{id:'contact',chain:'left',space:'world',position:[t.position.x-.06,t.position.y+.04,t.position.z+.1],positionWeight:.5,orientationWeight:0}]);
    }
    profiler.counter('M74 ordinary baked instances',this.targets.length,'latest');
  }
  presentationUpdate(){
    this.camera();
    if(!this.hud)return;
    const names=['SOURCE / original performance','BROAD / calibration + spacers','TALL / scaled translations + axes','INDEPENDENT / same broad asset'];
    let text=[`Root policy ${['preserve','extract','in-place'][this.state.root]} | masked layer ${this.state.layer} | shared IK ${this.state.ik} | physical contribution ${this.state.physical}`];
    for(const [i,e] of [this.source,...this.targets].entries())text.push(e?.animation.info.ready?`${names[i]}: ${e.animation.info.clip} ${e.animation.time.toFixed(2)}s`:`${names[i]} loading`);
    if(this.request){const s=saves.status(this.request);text.push(`Save/load: ${s?.state}${s?.error?' '+s.error:''}`);}
    this.hud.get('status').text=text.join('\n');
  }
  destroy(){if(this.region)scenes.releaseRegion(this.region);world.clearView();}
}
