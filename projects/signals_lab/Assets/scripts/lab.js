import {signals,world,input,ui,scenes,saves} from 'judas';
export const properties={prefab:{type:'string',default:''}};
export default class {
 constructor({properties}){this.props=properties;this.state={receipts:{},menuCount:0};}
 register(){
  this.hud=ui.get('signals_ui');ui.debugOverlayVisible=false;
  signals.subscribe('lab.receipt','ui');signals.subscribe('lab.menu','ui');
  this.hud.modal=false;this.hud.get('menu').visible=false;input.pointerCapture=false;
  world.setView({position:{x:0,y:5,z:12},rotation:{w:.98,x:-.2,y:0,z:0}},65);
 }
 start(){this.register();} restore(){this.register();}
 onSignal(e){
  if(e.name==='lab.receipt')this.state.receipts[e.payload.label]=e.payload;
  else {this.state.menuCount++;signals.emit('lab.pulse',{amount:1});}
 }
 broadcast(){this.last=signals.emit('lab.pulse',{amount:1});}
 targeted(){const target=world.entity('20');if(target)this.last=signals.send(target,'lab.pulse',{amount:1});}
 menu(show){this.hud.modal=show;this.hud.get('menu').visible=show;}
 uiUpdate(){
  if(input.pressed('pause'))this.menu(!this.hud.modal);
  if(input.pressed('broadcast'))this.broadcast();
  if(input.pressed('target'))this.targeted();
  if(input.pressed('toggle')){const e=world.entity('20');if(e)signals.send(e,'lab.toggle');}
  if(input.pressed('delete_receiver')){
   // Lookup constructs a wrapper; deletion is not idempotent on a stale entity.
   const receiver=world.entity('30');if(receiver?.valid)receiver.destroy();
  }
  if(input.pressed('spawn_receiver'))world.spawnPrefab(this.props.prefab,{position:{x:4,y:1,z:-2}});
  if(input.pressed('region')){
   if(this.regionToken){scenes.releaseRegion(this.regionToken);scenes.unloadRegion('annex');this.regionToken=null;}
   else this.regionToken=scenes.requestRegion('annex');
  }
  if(input.pressed('reset'))scenes.reload();
  if(input.pressed('save'))this.request=saves.save('signals',{name:'Signals Lab'});
  if(input.pressed('load'))this.request=saves.load('signals');
  if(this.request){const s=saves.status(this.request);this.status=`${s?.operation}: ${s?.state}${s?.error?' / '+s.error:''}`;}
  const receipts=Object.values(this.state.receipts).map(r=>`${r.label}: ${r.count} ${r.listening?'ON':'OFF'}${r.restores?' (restored)':''}`).join(' | ');
  const s=signals.stats;this.state.diag=s;
  this.hud.get('counts').text=receipts||'B broadcasts to all subscribed counters. T targets the left pair.';
  this.hud.get('stats').text=`Subscriptions ${s.subscriptions} | queued ${s.queuedEvents} / ${s.queuedRecipients} | delivered ${s.delivered} | UI previews ${this.state.menuCount}\n${this.status||'Transient notifications are not saved; counter facts are.'}`;
 }
 onUI(e){if(e.type==='back'||(e.type==='click'&&e.element==='resume'))this.menu(false);
  if(e.type==='click'&&e.element==='preview')signals.emit('lab.menu',{amount:1},'ui');
  if(e.type==='click'&&e.element==='pulse')this.broadcast();
 }
 destroy(){input.pointerCapture=false;world.clearView();}
}
