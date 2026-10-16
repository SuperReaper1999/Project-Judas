import {world,input,ui,scenes,saves,players,entity} from 'judas';
import {add,mul,norm,cross,look,sub} from './math.js';
export const properties={fullDetail:{type:'boolean',default:false}};
const centres=[{x:0,y:0,z:0},{x:180,y:0,z:0},{x:180,y:0,z:180},{x:0,y:0,z:180}];
const v=(x=0,y=0,z=0)=>({x,y,z});
export default class {
 constructor({properties}){this.props=properties;this.state={time:0,tour:true,district:0,split:false,monitors:true,eye:v(0,12,44),yaw:0,pitch:-.25};this.views=[];this.age=1;this.request=0;}
 start(){this.bind();} restore(){this.bind();}
 bind(){this.doc=ui.get('asset_lab');this.doc.modal=false;this.doc.get('pause').visible=false;ui.debugOverlayVisible=false;input.pointerCapture=true;this.views=players.list;this.split(this.state.split);this.remote();}
 remote(){for(let i=0;i<4;i++){const c=entity(String(40000+i));if(c?.valid)c.setCameraEnabled(this.state.monitors&&i===(this.state.district+1)%4);}}
 split(on){for(const p of this.views)if(p.valid)p.leave();this.views=[];this.state.split=on;if(on){this.views=[players.create('Museum view'),players.create('Next district')];this.views[0].viewport={x:0,y:0,width:.5,height:1};this.views[1].viewport={x:.5,y:0,width:.5,height:1};}}
 pause(on){this.doc.modal=on;this.doc.get('pause').visible=on;input.pointerCapture=!on;}
 uiUpdate(){this.openedPause=false;if(input.pressed('pause')&&!this.doc.modal){this.pause(true);this.openedPause=true;}}
 onUI(e){if(e.document!=='asset_lab')return;if((e.type==='back'&&!this.openedPause)||(e.type==='click'&&e.element==='resume'))this.pause(false);if(e.type==='click'&&e.element==='reload')scenes.reload();}
 update(dt){if(this.doc.modal)return;this.state.time+=dt;this.age+=dt;
  if(input.pressed('watch'))this.state.tour=!this.state.tour;
  if(input.pressed('split'))this.split(!this.state.split);
  if(input.pressed('monitors')){this.state.monitors=!this.state.monitors;this.remote();}
  if(input.pressed('detail'))scenes.load(this.props.fullDetail?'Scenes/museum.judas':'Scenes/full_detail.judas');
  for(let n=0;n<4;n++)if(input.pressed('district'+n)){this.state.district=n;this.state.tour=false;this.state.eye=add(centres[n],v(0,12,44));this.state.yaw=0;this.state.pitch=-.25;this.remote();}
  if(input.pressed('reset'))scenes.reload();if(input.pressed('save'))this.request=saves.save('museum');if(input.pressed('load'))this.request=saves.load('museum');
  if(!this.state.tour){this.state.yaw-=input.axis('look_x')*.002;this.state.pitch=Math.max(-1.4,Math.min(1.4,this.state.pitch-input.axis('look_y')*.002));const f=v(-Math.sin(this.state.yaw),0,-Math.cos(this.state.yaw)),right=cross(f,v(0,1,0));this.state.eye=add(this.state.eye,mul(norm(add(add(mul(f,input.axis('move_z')),mul(right,input.axis('move_x'))),v(0,input.axis('move_y'),0))),dt*25));}
  else{const n=Math.floor(this.state.time/20)%4;if(n!==this.state.district){this.state.district=n;this.remote();}}
  if(this.age>.4){this.age=0;let text=`DISTRICT ${this.state.district+1} / ${this.props.fullDetail?'FULL DETAIL CONTROL':'PROJECTED-ERROR LOD'} / ${this.state.tour?'WATCH TOUR':'FREE FLIGHT'}\n576 detailed sculptures / 16 cooked textures / 8 MiB visual budget\n${this.state.split?'Independent split views':'Main camera'} + ${this.state.monitors?'one cached remote camera':'remote cameras off'} / required dependencies remain pinned`;
   if(this.request){const s=saves.status(this.request);text+=`\nSave/load: ${s?.state||'pending'} ${s?.error||''}`;}this.doc.get('status').text=text;}
 }
 presentationUpdate(){let eye=this.state.eye,forward=v(-Math.sin(this.state.yaw)*Math.cos(this.state.pitch),Math.sin(this.state.pitch),-Math.cos(this.state.yaw)*Math.cos(this.state.pitch));if(this.state.tour){const n=this.state.district,p=this.state.time%20;let a=Math.max(0,Math.min(1,(p-10)/10));a=a*a*(3-2*a);const c=add(mul(centres[n],1-a),mul(centres[(n+1)%4],a));eye=add(c,v(38*Math.sin(this.state.time*.13),12,42*Math.cos(this.state.time*.13)));forward=norm(sub(add(c,v(0,2,0)),eye));}world.setView({position:eye,rotation:look(forward,v(0,1,0))},65,{near:.1,far:240});
  if(this.state.split)for(let i=0;i<this.views.length;i++){const p=i?add(centres[(this.state.district+1)%4],v(25,12,40)):eye;const f=i?norm(sub(add(centres[(this.state.district+1)%4],v(0,2,0)),p)):forward;this.views[i].setView({position:p,rotation:look(f,v(0,1,0))},65,{near:.1,far:240});}
 }
 destroy(){for(const p of this.views)if(p.valid)p.leave();input.pointerCapture=false;world.clearView();}
}
