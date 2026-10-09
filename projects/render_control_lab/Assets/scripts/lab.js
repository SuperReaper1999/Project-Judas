import {world,input,ui,scenes,entity,physics,saves} from 'judas';
const v=(x=0,y=0,z=0)=>({x,y,z});
const grid='470c6f3632444d0cf62bddca0cd64bfe',cutout='42f619a708d0f7677faae196b78100d3';
export default class {
 constructor(){this.position=v(0,4,15);this.yaw=0;this.pitch=-.12;this.state={time:0,run:false,mode:0,colour:false,texture:false,error:''};}
 start(){this.bind();}
 restore(){this.bind();} // saved render state is restored by Judas; never replay initialization
 bind(){ui.debugOverlayVisible=false;input.pointerCapture=true;this.hud=ui.get('lab_ui');}
 uiUpdate(){if(input.pressed('save'))this.request=saves.save('render-lab',{name:'Render controls'});if(input.pressed('load'))this.request=saves.load('render-lab');}
 update(dt){
  if(input.pressed('pause'))input.pointerCapture=!input.pointerCapture;if(input.pressed('restart'))scenes.reload();
  if(input.pointerCapture){this.yaw-=input.axis('look_x')*.002;this.pitch=Math.max(-1.3,Math.min(1.3,this.pitch-input.axis('look_y')*.002));}
  if(input.pressed('cycle'))this.state.run=!this.state.run;
  let scrub=(input.held('scrub_forward')?1:0)-(input.held('scrub_back')?1:0);
  if(this.state.run||scrub){this.state.time+=dt*(scrub? scrub*2:1);const t=this.state.time*.2;world.setAppearance({sunDirection:v(Math.sin(t),.2+.7*Math.cos(t),.4),sunIntensity:Math.max(0,.8+Math.cos(t)),sunColor:v(1,.65+.3*Math.max(0,Math.cos(t)),.45+.45*Math.max(0,Math.cos(t))),ambientColor:v(.04,.05,.07),environmentIntensity:.25+.5*Math.max(0,Math.cos(t)),backgroundColor:v(.025,.035,.065)});}
  if(input.pressed('sun'))world.setAppearance({sunEnabled:!world.appearance.sunEnabled});
  if(input.pressed('blocker_toggle'))world.setAppearance({environmentBackground:!world.appearance.environmentBackground});
  if(input.pressed('camera_toggle'))world.setAppearance({exposure:world.appearance.exposure>1?.7:2.5});
  const item=entity('40'),m=item.material('*');
  if(input.pressed('hide_entity'))for(const id of ['40','35','60']){const e=entity(id);e.renderVisible=!e.renderVisible;}
  if(input.pressed('hide_renderer'))item.rendererVisible=!item.rendererVisible;
  if(input.pressed('hide_part')&&item.modelParts){const p=item.modelParts[0];item.setPartVisible(p.identity,!p.visible);}
  if(input.pressed('color')){this.state.colour=!this.state.colour;m.set({baseColor:this.state.colour?{x:.15,y:.7,z:.25,a:1}:{x:1,y:1,z:1,a:1},roughness:.3,metallic:.1,emissive:v(.015,.06,.02)});}
  if(input.pressed('alpha')){this.state.mode=(this.state.mode+1)%3;m.set({alphaMode:['opaque','mask','blend'][this.state.mode],alphaCutoff:.5,baseColor:{x:.35,y:.8,z:.7,a:this.state.mode===2?.35:1},doubleSided:true,textures:{baseColor:this.state.mode===1?cutout:''}});}
  if(input.pressed('texture')){this.state.texture=!this.state.texture;m.set({textures:{baseColor:this.state.texture?grid:cutout}});}
  if(input.pressed('failed_texture')){try{m.set({roughness:.9,textures:{baseColor:'00000000000000000000000000000000'}});}catch(e){this.state.error=String(e);}}
  if(input.pressed('clear')){m.clearOverrides();for(const p of item.modelParts??[])item.material(p.identity).clearOverrides();world.resetAppearance();this.state.run=false;this.state.error='';}
  if(input.pressed('spawn_navigator'))world.spawnPrefab('44914741099d012af2c3eca432018951',{position:v(0,0,7)});
 }
 fixedUpdate(dt){let x=input.axis('move_x'),z=input.axis('move_y');this.position.x+=(Math.cos(this.yaw)*x-Math.sin(this.yaw)*z)*dt*5;this.position.z+=(-Math.sin(this.yaw)*x-Math.cos(this.yaw)*z)*dt*5;}
 presentationUpdate(){
  const figure=entity('35'),joint=figure.animation?.info.joints?.at(-1);
  if(joint&&!this.socketBound){entity('61').setSocket(figure,joint);this.socketBound=true;}
  const socket=joint&&figure.animation.jointTransform(joint,'world',true);
  const cy=Math.cos(this.yaw/2),sy=Math.sin(this.yaw/2),cp=Math.cos(this.pitch/2),sp=Math.sin(this.pitch/2);world.setView({position:this.position,rotation:{w:cy*cp,x:cy*sp,y:sy*cp,z:-sy*sp}},60);
  if(!this.hud)return;const a=world.appearance,m=entity('40').material('*').state;
  const hit=physics.raycast(v(0,1,10),v(0,0,-1),3,{ignoreEntities:[entity('40'),entity('41')]});
  this.hud.get('score').text=`Sun ${a.sunEnabled?'ON':'OFF'} ${a.sunIntensity.toFixed(2)} | fixed exposure ${a.exposure.toFixed(1)} | JS clock ${this.state.time.toFixed(1)} ${this.state.run?'running':'paused'}`;
  this.hud.get('progress').text=`Material ${m.alphaMode} / ${m.status} | hidden-solid query ${hit?.entity?.id??'miss'} | socket ${socket?'live':'waiting'} / script ${entity('60').scriptState(1)?.ticks??0} | ${this.state.error||this.request&&saves.status(this.request)?.state||'authored sources stay shared'}`;
  this.hud.get('controls').text='P cycle  Arrows scrub  F sun  H entity  J renderer  K part  C colour  O alpha  T texture  Y reject  Backspace reset  F6/F7 save/load';
 }
 destroy(){input.pointerCapture=false;world.clearView();}
}
