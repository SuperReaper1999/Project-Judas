import {signals,time} from 'judas';
export default class {
  /** @param {import("judas").ScriptContext} context */
  constructor({entity}) {this.entity=entity;this.previewPublished=false;this.state={count:0,preview:0,published:false,fixed:false,uiFixed:false};}
  register() {this.pulse=signals.subscribe('example.pulse');this.menu=signals.subscribe('example.preview','ui');}
  start() {this.register();}
  restore() {this.register();} // saved facts stay intact; tokens are fresh
  fixedUpdate() {
    if(this.state.published)return;
    this.state.published=true;
    signals.emit('example.pulse',{amount:1});
    signals.send(this.entity,'example.pulse',{amount:1});
  }
  uiUpdate() {
    if(!this.previewPublished){this.previewPublished=true;signals.emit('example.preview',{amount:1},'ui');}
  }
  /** @param {import("judas").SignalEvent} event */
  onSignal(event) {
    const payload=event.payload;
    if(!payload||typeof payload!=="object"||Array.isArray(payload)||typeof payload.amount!=="number")return;
    if(event.name==='example.pulse'){this.state.count+=payload.amount;this.state.fixed=time.fixed;}
    else {this.state.preview+=payload.amount;this.state.uiFixed=time.fixed;}
  }
}
