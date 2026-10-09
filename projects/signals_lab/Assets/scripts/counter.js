import {signals} from 'judas';
export const properties={label:{type:'string',default:'counter'}};
export default class {
 constructor({entity,properties}) {this.entity=entity;this.props=properties;this.state={count:0,listening:true,restores:0};}
 register() {
  if(this.state.listening)this.pulse=signals.subscribe('lab.pulse');
  this.control=signals.subscribe('lab.toggle');
 }
 start(){this.register();}
 restore(){this.state.restores++;this.register();}
 onSignal(event){
  if(event.name==='lab.toggle'){
   this.state.listening=!this.state.listening;
   if(this.state.listening)this.pulse=signals.subscribe('lab.pulse');else signals.unsubscribe(this.pulse);
  } else {
   this.state.count+=event.payload.amount;
   // Real renderer action, through generic M72 material overrides.
   this.entity.material().set({baseColor:{x:.15+(this.state.count%3)*.25,y:.8,z:.35,a:1}});
  }
  signals.emit('lab.receipt',{label:this.props.label,count:this.state.count,listening:this.state.listening,restores:this.state.restores},'ui');
 }
}
