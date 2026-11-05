import {world,input,saves,console} from 'judas';
export default class {
 constructor(){this.state={gold:0};this.request=0;this.reported=false;}
 start(){this.state.gold=37;console.log('SDK_NEW_GAME gold='+this.state.gold);}
 restore(){if(this.state.gold!==37)throw new Error('cold save lost progress');console.log('SDK_COLD_RESTORE_OK gold='+this.state.gold);this.request=0;}
 update(){
  if(input.pressed('save'))this.request=saves.save('qualification',{name:'Renderer SDK qualification'});
  if(input.pressed('load'))this.request=saves.load('qualification');
  if(this.request){const status=saves.status(this.request);if(status?.state==='failed')throw new Error(status.error);if(status?.state==='completed'&&!this.reported){console.log('SDK_SAVE_REQUEST_COMPLETED gold='+this.state.gold);this.reported=true;}}
 }
 presentationUpdate(){world.setView({position:{x:0,y:2,z:6},rotation:{w:1,x:0,y:0,z:0}},60,{near:.1,far:100});}
}
