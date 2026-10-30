import {input,ui,scenes} from 'judas';
export default class {
 constructor(){this.state={open:false};this.doc=null;}
 start(){this.doc=ui.get('grove_pause');this.show(false);}
 restore(){this.start();}
 show(open){if(!this.doc)return;this.state.open=open;
  this.doc.visible=open;this.doc.modal=open;input.pointerCapture=false;
  if(open)this.doc.focus('resume');
 }
 uiUpdate(){if(input.pressed('pause'))this.show(!this.state.open);}
 onUI(event){
  if(event.document!=='grove_pause')return;
  if(event.type==='back'){this.show(false);return;}
  if(event.type!=='click')return;
  if(event.element==='resume')this.show(false);
  if(event.element==='retry'){this.show(false);scenes.reload();}
  if(event.element==='quit')ui.quit();
 }
 destroy(){this.show(false);}
}
