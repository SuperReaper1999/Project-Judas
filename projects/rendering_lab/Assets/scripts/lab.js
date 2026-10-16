import {world,input,ui,scenes,saves,players} from 'judas';
import {add,mul,norm,cross,look,sub} from './math.js';
const v=(x=0,y=0,z=0)=>({x,y,z});
export default class {
 constructor(){this.state={time:0,tour:true,lights:true,shadows:true,split:false,eye:v(0,12,42),yaw:0,pitch:-.2};this.views=[];this.request=0;this.age=1;}
 start(){this.bind();}
 restore(){this.bind();}
 bind(){this.doc=ui.get('rendering_lab');this.doc.modal=false;this.doc.get('pause').visible=false;ui.debugOverlayVisible=false;input.pointerCapture=true;this.lamps=world.queryTags(['lamp']);this.views=players.list;this.setLights();this.split(this.state.split);}
 setLights(){for(const lamp of this.lamps)if(lamp.valid)lamp.setLight({enabled:this.state.lights,castsShadow:this.state.shadows&&lamp.light.kind==='spot'});}
 split(enabled){this.state.split=enabled;for(const p of this.views)if(p.valid)p.leave();this.views=[];if(enabled){this.views=[players.create('West gallery'),players.create('East gallery')];this.views[0].viewport={x:0,y:0,width:.5,height:1};this.views[1].viewport={x:.5,y:0,width:.5,height:1};}}
 pause(open){this.doc.modal=open;this.doc.get('pause').visible=open;input.pointerCapture=!open;}
 uiUpdate(){this.openedPause=false;if(input.pressed('pause')&&!this.doc.modal){this.pause(true);this.openedPause=true;}}
 onUI(e){if(e.document!=='rendering_lab')return;if((e.type==='back'&&!this.openedPause)||(e.type==='click'&&e.element==='resume'))this.pause(false);if(e.type==='click'&&e.element==='reload')scenes.reload();}
 update(dt){if(this.doc.modal)return;this.state.time+=dt;this.age+=dt;
  if(input.pressed('watch'))this.state.tour=!this.state.tour;
  if(input.pressed('lights')){this.state.lights=!this.state.lights;this.setLights();}
  if(input.pressed('shadows')){this.state.shadows=!this.state.shadows;this.setLights();}
  if(input.pressed('split'))this.split(!this.state.split);
  for(const [key,scene] of [['count_small','small'],['count_normal','gallery'],['count_dense','dense']])if(input.pressed(key))scenes.load('Scenes/'+scene+'.judas');
  if(input.pressed('reset'))scenes.reload();if(input.pressed('save'))this.request=saves.save('rendering-lab',{name:'Light Factory'});if(input.pressed('load'))this.request=saves.load('rendering-lab');
  this.state.yaw-=input.axis('look_x')*.002;this.state.pitch=Math.max(-1.4,Math.min(1.4,this.state.pitch-input.axis('look_y')*.002));
  const f=v(-Math.sin(this.state.yaw),0,-Math.cos(this.state.yaw)),right=cross(f,v(0,1,0));this.state.eye=add(this.state.eye,mul(norm(add(add(mul(f,input.axis('move_z')),mul(right,input.axis('move_x'))),v(0,input.axis('move_y'),0))),dt*16));
  // Deliberately script individual authored sources, not renderer internals.
  for(let i=0;i<16;i++)if(this.lamps[i]?.valid)this.lamps[i].setLight({intensity:.8+.4*Math.sin(this.state.time*1.2+i*.5)});
  if(this.age>.3){this.age=0;let text=`${this.lamps.length} authored lights / ${this.state.lights?'ON':'OFF'} / shadow requests ${this.state.shadows?'ON':'OFF'}\n${this.state.split?'TWO INDEPENDENT PLAYER VIEWS':'MAIN VIEW + TWO LIVE CAMERA MONITORS'} / ${this.state.tour?'WATCH TOUR':'FREE CAMERA'}\nEach camera selects its own light tiles and shadow coverage. F5 hands you the camera.`;if(this.request){const s=saves.status(this.request);text+=`\nSave/load: ${s?.state||'pending'} ${s?.error||''}`;}this.doc.get('status').text=text;}
 }
 presentationUpdate(){let eye=this.state.eye,forward=v(-Math.sin(this.state.yaw)*Math.cos(this.state.pitch),Math.sin(this.state.pitch),-Math.cos(this.state.yaw)*Math.cos(this.state.pitch));if(this.state.tour){let t=this.state.time*.09;eye=v(31*Math.sin(t),10+3*Math.sin(t*.7),31*Math.cos(t));forward=norm(mul(eye,-1));}world.setView({position:eye,rotation:look(forward,v(0,1,0))},65,{near:.1,far:240});
  if(this.state.split)for(let i=0;i<this.views.length;i++){let p=i?mul(eye,-1):eye;p={...p,y:12};this.views[i].setView({position:p,rotation:look(norm(sub(v(0,1,0),p)),v(0,1,0))},65,{near:.1,far:240});}
 }
 destroy(){for(const p of this.views)if(p.valid)p.leave();input.pointerCapture=false;world.clearView();}
}
