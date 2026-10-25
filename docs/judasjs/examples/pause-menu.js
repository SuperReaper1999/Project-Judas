// Optional project example. Logical controls remain owned by the project.
import {input, ui, scenes, console} from 'judas';
/** @typedef {import('judas').PlayerInputEntry} Entry */
/** @typedef {{name:string,binding:number,vector:boolean,label:string}} BindingRow */
const keyChoices = [
  ...'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'.split('').map(x=>'key:'+x),
  ...['Space','Escape','Return','Tab','Backspace','Up','Down','Left','Right','Left Shift','Left Ctrl'].map(x=>'key:'+x),
  ...Array.from({length:12},(_,n)=>'key:F'+(n+1)),
  ...['Left','Right','Middle','X1','X2'].map(x=>'mouse:'+x),
  ...['South','East','West','North','Start','Back','LeftShoulder','RightShoulder','LeftStick','RightStick','DpadUp','DpadDown','DpadLeft','DpadRight'].map(x=>'pad:'+x),
  ...['LeftX','LeftY','RightX','RightY','LeftTrigger','RightTrigger'].map(x=>'stick:'+x),
  'mouse:dx','mouse:dy','mouse:wheelX','mouse:wheelY'
];

export default class {
  constructor() {
    /** @type {{open:boolean,bindings:Entry[]|null}} */
    this.state={open:false,bindings:null};
    /** @type {import('judas').UIDocument|null} */
    this.doc=null;
    /** @type {BindingRow[]} */ this.rows=[];
    /** @type {string[]} */ this.choices=[];
    this.selected=0;this.chosen=-1;this.page='pause';this.refreshPending=false;this.restoreBindings=false;
  }
  start() {
    this.doc=ui.get('pause_menu');
    if(!this.doc)throw new Error('Pause menu UI is missing. Keep its UI component and script together.');
    // Restore can run before an input snapshot exists. Publish from uiUpdate.
    this.restoreBindings=this.state.bindings!==null;
    this.show(false);
  }
  restore() {this.start();}
  /** @param {boolean} open */
  show(open) {
    if(!this.doc)return;
    this.state.open=open;this.doc.visible=open;this.doc.modal=open;
    input.pointerCapture=!open;
    if(open){this.page='pause';this.doc.get('pause').visible=true;this.doc.get('settings').visible=false;this.doc.focus('resume');}
  }
  uiUpdate() {
    if(this.restoreBindings){
      try{if(input.setBindings(this.state.bindings))this.restoreBindings=false;}
      catch(error){this.restoreBindings=false;console.log('Saved controls do not match this project:',String(error));}
    }
    if(input.pressed('pause'))this.show(!this.state.open);
    if(this.state.open)input.pointerCapture=false;
    if(this.refreshPending){this.refreshPending=false;this.populate();}
  }
  populate() {
    const doc=this.doc;if(!doc)return;
    this.rows=[];
    for(const entry of input.getBindings()){
      const count=Math.max(1,entry.bindings.length);
      for(let n=0;n<count&&this.rows.length<10000;++n){
        const binding=entry.bindings[n];
        this.rows.push({name:entry.name,binding:n,vector:!!entry.vector,
          label:entry.name.replace(/_/g,' ')+'  /  '+(binding?binding.control:'Unbound')+(binding&&binding.scale<0?' (negative)':'')});
      }
    }
    doc.get('bindings').setItems(this.rows.map((row,n)=>({id:String(n),text:row.label,enabled:!row.name.startsWith('ui_')})));
    doc.get('bindings').visible=true;doc.get('choices').visible=false;
    doc.get('filter').visible=false;doc.get('change').visible=true;
    doc.get('apply').visible=false;
    doc.get('status').text='Select a binding, then Change. UI navigation is protected in this example.';
    this.selected=0;
  }
  settings() {
    const doc=this.doc;if(!doc)return;
    this.page='settings';doc.get('pause').visible=false;doc.get('settings').visible=true;
    this.populate();doc.focus('bindings');
  }
  chooseBinding() {
    const doc=this.doc,row=this.rows[this.selected];if(!doc||!row||row.name.startsWith('ui_'))return;
    this.page='choices';doc.get('bindings').visible=false;doc.get('choices').visible=true;
    doc.get('filter').visible=true;doc.get('filter').text='';doc.get('change').visible=false;
    doc.get('apply').visible=true;doc.get('apply').enabled=false;this.chosen=-1;
    this.choices=row.vector?['stick:Left','stick:Right']:keyChoices;
    this.filter('');doc.get('status').text='Choose a physical control for '+row.name+'.';doc.focus('choices');
  }
  /** @param {string} query */
  filter(query) {
    const doc=this.doc;if(!doc)return;
    const q=query.toLowerCase();
    doc.get('choices').setItems(this.choices.map((control,n)=>({id:String(n),text:control})).filter(x=>x.text.toLowerCase().includes(q)));
  }
  /** @param {number} index */
  apply(index) {
    const row=this.rows[this.selected],control=this.choices[index];if(!row||!control)return;
    const map=input.getBindings(),entry=map.find(x=>x.name===row.name);if(!entry)return;
    const binding=entry.bindings[row.binding]||{control,scale:1,deadzone:0};
    entry.bindings[row.binding]={...binding,control,...(row.vector?{scaleY:binding.scaleY??1,circular:binding.circular??true}:{})};
    input.setBindings(map);this.state.bindings=map;this.refreshPending=true;this.page='settings';
  }
  /** @param {import('judas').UIEvent} event */
  onUI(event) {
    const doc=this.doc;if(!doc||event.document!=='pause_menu')return;
    if(event.type==='back'){this.show(false);return;}
    if(event.type==='click'){
      if(event.element==='resume'||event.element==='tab_pause'){this.page='pause';doc.get('settings').visible=false;doc.get('pause').visible=true;if(event.element==='resume')this.show(false);}
      if(event.element==='tab_settings')this.settings();
      if(event.element==='change')this.chooseBinding();
      if(event.element==='apply'&&this.chosen>=0)this.apply(this.chosen);
      if(event.element==='reset'){input.setBindings(null);this.state.bindings=null;this.refreshPending=true;}
      if(event.element==='restart')scenes.reload();
      if(event.element==='quit')ui.quit();
    }
    if(event.type==='change'){
      if(event.element==='bindings'&&event.item)this.selected=Number(event.item);
      if(event.element==='choices'&&event.item){this.chosen=Number(event.item);doc.get('apply').enabled=true;}
      if(event.element==='filter')this.filter(event.text);
    }
  }
  destroy() {if(this.doc){this.doc.modal=false;this.doc.visible=false;}input.pointerCapture=false;}
}
