import {ui} from 'judas';
// Fixture asset: Field Notes' ordinary .judasui document.
export const properties={asset:{type:'string',default:'20bc1b3dc528ff0d88abdec9e244789f'}};
export default class Controls {
 /** @param {import('judas').ScriptContext<{asset:string}>} context */
 constructor({properties}){this.props=properties;this.state={configured:false,restored:false,invalid:false,submitted:false,stale:false,inputBounds:true};}
 start(){
  this.doc=ui.load(this.props.asset,'controls');this.doc.modal=false;
  this.doc.get('menu').visible=false;this.doc.get('entry').visible=true;
  this.name=this.doc.get('name');this.name.configure({placeholder:'Callsign',maxBytes:128});
  this.name.text='café';this.name.selection={anchor:0,caret:4};
  this.doc.get('items').setItems([{id:'lamp',text:'Survey lamp'},{id:'rope',text:'Climbing rope'}]);
  this.doc.get('items').selectedIndex=1;this.doc.scale=1.1;
  const saved=this.doc.captureState();this.name.text='changed';this.doc.restoreState(saved);
  this.state.restored=this.name.text==='café'&&this.doc.get('items').selectedIndex===1;
  this.state.configured=this.doc.accessibility.some(e=>e.role==='textbox');
  try{this.name.selection={anchor:0,caret:10000};}catch(e){this.state.invalid=e instanceof TypeError;}
  // @ts-expect-error Intentional invalid-version rejection probe.
  try{this.doc.restoreState({...saved,version:999});}catch(e){this.state.invalid&&=e instanceof TypeError&&this.name.text==='café';}
  // Rejected editable-field replacements leave the previous value intact.
  for(const value of ['A\0B','line\nfeed','line\rreturn','x'.repeat(129)]){
   try{this.name.text=value;this.state.inputBounds=false;}
   catch(e){this.state.inputBounds&&=e instanceof TypeError&&this.name.text==='café';}
  }
  this.name.selection={anchor:4,caret:4};this.doc.focus('name');
 }
 /** @param {import('judas').UIEvent} event */
 onUI(event){if(event.document==='controls'&&event.type==='submit')this.state.submitted=event.text==='café!';}
 update(){if(this.state.submitted&&!this.state.stale&&this.doc&&this.name){this.doc.unload();try{this.name.text;}catch(e){this.state.stale=e instanceof ReferenceError;}}}
}
