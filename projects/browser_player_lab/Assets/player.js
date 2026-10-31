import Motor, {properties} from './motor.js';
import {input,world,ui,saves,console} from 'judas';
export {properties};
export default class extends Motor {
 constructor(context){super(context);Object.assign(this.state,{time:0,clicks:0});this.sequence=0;this.request=null;this.restored=false;}
 restore(){super.restore();this.restored=true;}
 fixedUpdate(dt){super.fixedUpdate(dt);this.state.time+=dt;}
 onUI(e){if(e.type==='click'&&e.element==='test-click')this.state.clicks++;}
 uiUpdate(){
  super.uiUpdate();
  if(!saves.busy){
   if(input.pressed('save_game'))this.request=saves.save('player');
   if(input.pressed('load_game'))this.request=saves.load('player');
  }
  const status=this.request?saves.status(this.request):null;
  const doc=ui.get('browser_player');
  if(doc)doc.get('help').text='3D BROWSER PLAYER | WASD: move | Mouse: look | Space: jump | V: third person\n'+
   'Escape: release/capture mouse | K/L: save/load | Fullscreen: page button\n'+
   'Clicks: '+this.state.clicks+' | '+(status?'Save: '+status.state:'Push the crates and climb the steps.');
  // Public gameplay observations, separate from the browser backend telemetry.
  if(++this.sequence%5===0)console.log('PLAYER_PROBE '+JSON.stringify({sequence:this.sequence,
   state:this.state,position:this.entity.transform.position,velocity:this.entity.character.velocity,
   supported:this.entity.character.state.supported,viewport:world.viewport,status,restored:this.restored,busy:saves.busy}));
 }
}
