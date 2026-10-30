import {input,scenes,saves,ui} from 'judas';
export default class {
 constructor(){this.state={visits:0,request:null};}
 update(){
   if(input.pressed('platformer'))scenes.load('Scenes/main.judas');
   if(input.pressed('rotated'))scenes.load('Scenes/rotated.judas');
   if(input.pressed('topdown'))scenes.load('Scenes/topdown.judas');
   if(input.pressed('restart'))scenes.reload();
   if(input.pressed('save_game'))this.state.request=saves.save('playground');
   if(input.pressed('load_game'))this.state.request=saves.load('playground');
   const doc=ui.get('sprite_help');
   if(doc){const status=this.state.request?saves.status(this.state.request):null;
     doc.get('help').text='PLANAR PLAYGROUND | WASD/arrows | Space: jump | 1: platformer | 2: top-down | 3: tilted plane | R: restart\nF6/F7: save/load | Green beacon visits: '+this.state.visits+(status?' | '+status.state+' '+status.error:'');}
 }
 onTriggerEnter(event){if(event.other?.body2D?.kind==='capsule'&&event.other.body2D.lockRotation)this.state.visits++;}
}