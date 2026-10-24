// Gameplay controls only. No interface releases, scripted collapse or debris.
import {world,input,ui,scenes,saves,physics} from 'judas';
import {mul,add} from './math.js';
export default class {
 constructor(){this.state={load:false,right:false,middle:false,spawned:[],region:false};this.pending=[];this.region=0;this.request=0;this.message='A / B / C compare strength and support. Orange blocks are ordinary physical loads.';}
 start(){this.hud=ui.get('lab');if(this.hud){this.hud.get('pause').visible=false;this.hud.modal=false;}}
 restore(){this.start();if(this.state.region)this.region=scenes.requestRegion('annex');}
 owners(){return ['100','200','300','400',...this.state.spawned].map(id=>world.entity(id)).filter(e=>e?.valid&&e.fracture);}
 update(){if(this.hud?.modal)return;for(const a of ['load','release','adopt','push','spawn','region'])if(input.pressed(a))this.pending.push(a);
  if(input.pressed('flat'))scenes.load('Scenes/flat.judas');if(input.pressed('radial'))scenes.load('Scenes/radial.judas');if(input.pressed('restart'))scenes.reload();}
 // Take the queue first: one failed action must not stay queued and block later keys.
 fixedUpdate(){const pending=this.pending;this.pending=[];for(const a of pending){
   if(a==='load')this.state.load=!this.state.load;
   if(a==='release'){this.state.right=true;for(const id of ['501','511','521'])this.remove(id);for(const id of ['100','200','300'])world.entity(id)?.deformable?.release('right');this.message='Right support anchors removed. Physics alone decides remaining connection failure.';}
   if(a==='adopt'){world.entity('300')?.deformable?.release('middle');this.remove('523');this.state.middle=true;}
   if(a==='push'){const r=world.viewRay;if(r){const h=physics.raycast(r.origin,r.direction,60,{ignored:[world.entity('10')]});const e=h?.entity;if(e?.valid&&e.motionType==='dynamic'&&e.mass>0){e.applyImpulseAtPoint(mul(r.direction,500),h.point);this.message='500 N.s at the actual hit point.';}}}
   if(a==='spawn'&&this.state.spawned.length<4){const e=world.spawnPrefab('79797979797979797979797979790006',{position:{x:0,y:7,z:-7-this.state.spawned.length*3},rotation:{w:1,x:0,y:0,z:0},scale:{x:1,y:1,z:1}});this.state.spawned.push(e.id);}
   if(a==='region'){if(this.region){scenes.releaseRegion(this.region);this.region=0;this.state.region=false;}else{this.region=scenes.requestRegion('annex');this.state.region=true;}}
  }
  if(this.state.load)for(const id of ['100','200','300']){const e=world.entity(id);if(e?.fracture){const g=physics.gravity(e.transform.position);e.fracture.force(4,mul(g,2000));}}
 }
 // Pressing R or H again finds the support already removed.
 remove(id){const e=world.entity(id);if(e?.valid)e.destroy();}
 uiUpdate(){if(!this.hud)return;if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}
  if(input.pressed('save_game'))this.command('save');if(input.pressed('load_game'))this.command('load');
  if(this.request){const s=saves.status(this.request);if(s&&['completed','failed','cancelled'].includes(s.state)){this.message=s.state+(s.error?' — '+s.error:'');this.request=0;}}
  this.hud.get('title').text='JUDAS / LOAD-BEARING STRUCTURES';
  this.hud.get('score').text=this.owners().map((e,i)=>`${['A','B','C','D'][i]||'Prefab'}: ${e.fracture.state.interfaces.filter(b=>b.broken).length}/7 failed`).join('  |  ');
  this.hud.get('progress').text=`A:120kPa  B:1MPa  C:120kPa + middle pier | overload ${this.state.load?'2000kg equivalent':'off'} | right supports ${this.state.right?'released':'present'}`;
  this.hud.get('message').text=this.owners().map(e=>e.deformable.state.error||e.fracture.state.error).find(Boolean)||this.message;
 }
 command(op){if(!this.request)this.request=op==='save'?saves.save('structure-current',{name:'Structure topology and motion'}):op==='load'?saves.load('structure-current'):saves.delete('structure-current');}
 onUI(e){if(e.type==='back'||e.type==='click'&&e.element==='resume'){this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;}if(e.type==='click'&&e.element==='restart')scenes.reload();if(e.type==='click'&&e.element==='quit')ui.quit();if(e.type==='click'&&['save','load','delete'].includes(e.element))this.command(e.element);}
 destroy(){if(this.region)scenes.releaseRegion(this.region);}
}
