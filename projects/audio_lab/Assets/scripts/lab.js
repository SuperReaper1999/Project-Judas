import {world,input,ui,audio,physics,scenes} from 'judas';
import {add,mul,axis,qm,rotate,norm,length} from './math.js';
// Project policy only: movement/look, demo comparisons, routing and physical door choice.
export default class {
 constructor({entity}){this.entity=entity;this.yaw=0;this.pitch=0;this.clock=0;this.open=false;this.doppler=true;this.bypass=false;this.toneOn=true;this.frames=0;this.state={started:false};}
 start(){this.doc=ui.get('audio_lab');this.music=world.entity('20');this.tone=world.entity('21');this.noise=world.entity('22');this.pulse=world.entity('23');this.click=world.entity('24');this.hinge=physics.joint(world.entity('34'));input.pointerCapture=true;this.state.started=true;}
 menu(open){this.doc.modal=open;this.doc.get('menu').visible=open;input.pointerCapture=!open;}
 action(name){
  if(name==='resume')this.menu(false);
  if(name==='music'){const s=this.music.audio;if(s?.state==='paused')this.music.resumeAudio();else this.music.pauseAudio();}
  if(name==='loop')this.music.setAudio({loop:!this.music.audio.loop});
  if(name==='seek'){const s=this.music.audio;if(s?.duration)this.music.seekAudio(((s.position||0)+30)%s.duration);}
  if(name==='doppler'){this.doppler=!this.doppler;this.tone.setAudio({doppler:this.doppler?1:0});}
  if(name==='bypass'){this.bypass=!this.bypass;this.noise.setAudio({bypass:this.bypass});this.pulse.setAudio({bypass:this.bypass});}
  if(name==='door')this.open=!this.open;
  if(name==='effects'){const g=audio.group('effects');audio.setGroup('effects',{paused:!g.paused});}
  if(name==='tone'){this.toneOn=!this.toneOn;this.toneOn?this.tone.playAudio():this.tone.stopAudio();}
  if(name==='pulse'&&this.pulse.audio?.ready)this.pulse.playAudioOneShot();
  if(name==='reload')scenes.reload();
 }
 uiUpdate(){if(input.pressed('pause'))this.menu(!this.doc.modal);
  if(++this.frames%10===0){const d=audio.diagnostics,s=this.music.audio,n=this.noise.audio;this.doc.get('diagnostics').text=`Music ${s?.state} ${(s?.position||0).toFixed(1)} / ${(s?.duration||0).toFixed(1)} sec | Doppler ${this.doppler?'ON':'OFF'} | bypass ${this.bypass?'ON':'OFF'} | door ${this.open?'OPEN':'CLOSED'}\nVoices ${d?.voices} | Streams ${d?.streams} | PCM ${d?.streamBytes} bytes | underruns ${d?.underruns} | retirements ${d?.pendingRetirements} | reverb ${d?.reverbProcessors} | rays ${d?.occlusionQueries}\nWall source gain ${(n?.occlusionGain||0).toFixed(2)} / cutoff ${Math.round(n?.cutoff||0)} Hz. Esc: controls. Space: wet impulse.`;}
 }
 update(dt){if(this.doc.modal)return;this.yaw-=input.axis('look_x')*.002;this.pitch=Math.max(-1.4,Math.min(1.4,this.pitch-input.axis('look_y')*.002));
  for(const name of ['door','doppler','bypass','tone','pulse'])if(input.pressed(name))this.action(name);if(input.pressed('restart'))this.action('reload');this.clock+=dt;
  this.tone.transform={position:{x:16*Math.sin(this.clock*2),y:2,z:-4}};
 }
 fixedUpdate(){if(this.doc.modal)return;const motor=this.entity.character,q=axis({x:0,y:1,z:0},this.yaw),f=rotate(q,{x:0,y:0,z:-1}),r=rotate(q,{x:1,y:0,z:0});let direction=add(mul(f,input.axis('move_y')),mul(r,input.axis('move_x')));if(length(direction)>1)direction=norm(direction);const v=motor.velocity;motor.velocity={x:direction.x*5,y:v.y,z:direction.z*5};
  const coordinate=this.hinge.state.coordinate;this.hinge.setMotor(Math.max(-1.5,Math.min(1.5,((this.open?-1.5:0)-coordinate)*4)),80);
 }
 presentationUpdate(){const t=this.entity.presentedTransform,q=qm(axis({x:0,y:1,z:0},this.yaw),axis({x:1,y:0,z:0},this.pitch));world.setView({position:add(t.position,{x:0,y:.65,z:0}),rotation:q});}
 onUI(e){if(e.document!=='audio_lab')return;if(e.type==='back')this.menu(false);if(e.type==='click'){this.action(e.element);if(this.click.audio?.ready)this.click.playAudioOneShot();}if(e.type==='change'&&e.element==='gain')audio.setGroup('music',{gain:e.value},.2);}
 destroy(){input.pointerCapture=false;world.clearView();}
}
