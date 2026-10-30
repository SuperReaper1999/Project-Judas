import {input,saves,scenes,session,ui} from 'judas';
export default class {
 constructor(){this.state={request:null};}
 uiUpdate(){
  if(!saves.busy){if(input.pressed('side_view'))scenes.load('Scenes/main.judas');
   else if(input.pressed('top_view'))scenes.load('Scenes/village.judas');
   else if(input.pressed('restart'))scenes.reload();
   else if(input.pressed('save_game'))this.state.request=saves.save('tilemap');
   else if(input.pressed('load_game'))this.state.request=saves.load('tilemap');}
  const doc=ui.get('sprite_help'),status=this.state.request?saves.status(this.state.request):null;
  if(doc)doc.get('help').text='TILEMAP TRAIL | WASD / arrows | Space: jump | 1: Trail | 2: Village | R: restart\nFind the stars! '+(session.get('stars')??0)+' collected | F6: save | F7: load'+(status?' | '+status.state+' '+status.error:'');
 }
}