// Ordinary project policy: load, unload and observe. Judas owns shape and failure.
import {world,input,ui,scenes,saves,physics} from 'judas';
import {mul,norm} from './math.js';
const prefab={elastic:'80808080808080808080808000000008',plastic:'80808080808080808080808000000009'};
const beamLoad=40; // Project-authored total end force, N; not a material rule.
const failureLoads=[0,40,200,800];
export default class {
 constructor(){this.state={load:false,level:0,spawned:[],region:false};this.pending=[];this.region=0;this.request=0;this.message='B loads A and B equally. Unload and compare recovery. H increases the C load; no scripted damage.';}
 start(){this.hud=ui.get('lab');if(this.hud){this.hud.get('pause').visible=false;this.hud.modal=false;}}
 restore(){this.start();this.pending=[];this.region=0;if(this.state.region)this.region=scenes.requestRegion('annex');}
 owners(){return ['100','200','300','400','450',...this.state.spawned].map(id=>world.entity(id)).filter(e=>e?.valid&&e.deformable);}
 update(){if(this.hud?.modal)return;
  for(const action of ['load','adopt','push','spawn','region'])if(input.pressed(action))this.pending.push(action);
  if(input.pressed('flat'))scenes.load('Scenes/flat.judas');if(input.pressed('radial'))scenes.load('Scenes/radial.judas');if(input.pressed('restart'))scenes.reload();
 }
 fixedUpdate(){for(const action of this.pending){
   if(action==='load'){this.state.load=!this.state.load;this.message=this.state.load?`A and B: equal ${beamLoad} N end loads. Judas solves the resulting strain.`:'A and B unloaded. Elastic strain recovers; plastic rest strain remains.';}
   if(action==='adopt'){this.state.level=(this.state.level+1)%failureLoads.length;this.message=`C end load: ${failureLoads[this.state.level]} N. Actual section traction decides irreversible failure.`;}
   if(action==='push'){
    const ray=world.viewRay;let nearest=null,patch=null;
    if(ray)for(const owner of this.owners()){const hit=owner.deformable.raycast(ray.origin,ray.direction,40);if(hit&&(!nearest||hit.distance<nearest.distance)){nearest=hit;patch=owner.deformable;}}
    if(nearest){patch.impulseAt(nearest.location,mul(ray.direction,2));this.message='2 N.s at the actual deformed hit location.';}
   }
   if(action==='spawn'&&this.state.spawned.length<4){
    const kind=this.state.spawned.length%2?'plastic':'elastic';
    const entity=world.spawnPrefab(prefab[kind],{position:{x:0,y:3,z:-5-this.state.spawned.length*3},rotation:{w:1,x:0,y:0,z:0},scale:{x:1,y:1,z:1}});
    this.state.spawned.push(entity.id);this.message=`Independent ${kind} prefab created.`;
   }
   if(action==='region'){if(this.region){scenes.releaseRegion(this.region);this.region=0;this.state.region=false;}else{this.region=scenes.requestRegion('annex');this.state.region=true;}}
  }this.pending=[];
  if(this.state.load)for(const id of ['100','200']){const entity=world.entity(id);if(entity?.deformable)entity.deformable.force(mul(norm(physics.gravity(entity.transform.position)),beamLoad),'right');}
  const structure=world.entity('300');if(structure?.deformable&&failureLoads[this.state.level])structure.deformable.force(mul(norm(physics.gravity(structure.transform.position)),failureLoads[this.state.level]),'right');
 }
 uiUpdate(){if(!this.hud)return;
  if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;input.pointerCapture=!this.hud.modal;}
  if(input.pressed('save_game'))this.command('save');if(input.pressed('load_game'))this.command('load');
  if(this.request){const status=saves.status(this.request);if(status&&['completed','failed','cancelled'].includes(status.state)){this.message=status.state+(status.error?' — '+status.error:'');this.request=0;}}
  let contacts=0,nodes=0;const errors=[];
  for(const entity of this.owners()){const state=entity.deformable.state;nodes+=state.nodes;contacts+=state.contacts;if(state.error)errors.push(state.error);}
  const fractured=world.entity('300')?.fracture?.state.interfaces.filter(i=>i.broken).length||0;
  this.hud.get('score').text='A GREEN elastic | B ORANGE plastic | C BLUE cohesive | D PURPLE panel | E oblique';
  this.hud.get('progress').text=`A/B end loads ${this.state.load?`${beamLoad} N`:'off'} | C ${failureLoads[this.state.level]} N / ${fractured} failed | ${nodes} nodes / ${contacts} contacts`;
  this.hud.get('message').text=errors[0]||this.message;
 }
 command(operation){if(this.request)return;this.request=operation==='save'?saves.save('bending-current',{name:'Deformed geometry and plastic rest state'}):operation==='load'?saves.load('bending-current'):saves.delete('bending-current');}
 onUI(event){if(event.type==='back'||event.type==='click'&&event.element==='resume'){this.hud.modal=false;this.hud.get('pause').visible=false;input.pointerCapture=true;}
  if(event.type==='click'&&event.element==='restart')scenes.reload();if(event.type==='click'&&event.element==='quit')ui.quit();if(event.type==='click'&&['save','load','delete'].includes(event.element))this.command(event.element);
 }
 destroy(){if(this.region)scenes.releaseRegion(this.region);}
}
