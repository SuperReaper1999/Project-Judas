// All needs, damage, movement, recovery and rewards are ordinary game policy.
// The engine chooses actions; no action is selected by the watch-tour script.
import {UtilityAgent,entity,input,physics,world,ui,saves,scenes} from 'judas';
import citizenDefinition from './citizen.ai.js';
import guardDefinition from './guard.ai.js';

const zero={x:0,y:0,z:0};
const clamp=v=>Math.max(0,Math.min(1,v));
const distance=(a,b)=>Math.hypot(a.x-b.x,a.y-b.y);
const target=id=>entity(String(id));
const percent=v=>Math.round(v*100)+'%';
const targetNames={'101':'west canteen','102':'east canteen','103':'garden bed','104':'garden bench','105':'meeting tree','201':'west shelter','202':'east shelter','203':'south patrol','204':'north patrol'};

/** Cast-checked movement uses MK2's actual kinematic endpoint; no pose snaps. */
function walk(actor,destination,dt,speed=2.2){
 if(!actor?.valid||!destination?.valid)return false;
 const pose=actor.transform,p=pose.position,t=destination.transform.position;
 const length=distance(p,t);
 if(length<.12){actor.setKinematicVelocity(zero,zero);return true;}
 const direction={x:(t.x-p.x)/length,y:(t.y-p.y)/length,z:0};
 const travel=Math.min(speed*dt,Math.max(0,length-.08));
 const hit=physics.cast2D({kind:'box',halfExtents:{x:.23,y:.23}},pose,direction,travel,{ignored:[actor],includeSensors:false});
 if(hit&&hit.distance<travel-1e-5){actor.setKinematicVelocity(zero,zero);return false;}
 actor.setKinematicVelocity({x:direction.x*travel/dt,y:direction.y*travel/dt,z:0},zero);
 actor.setSprite({flipX:direction.x<0});
 return false;
}

export default class {
 constructor(){
  this.state={time:0,paused:false,watch:true,tourStage:0,health:1,hunger:.72,fatigue:.44,company:.25,
   pantry:7,meals:0,rested:0,chats:0,shelters:0,damage:0,decisions:0,
   request:null,traceSecond:0,services:{eat:0,rest:0,socialize:0},message:'Watch the villagers choose, then change their needs with H / T / C.',
   citizen:null,guard:null};
  this.brains=null;this.actors=null;
 }
 start(){this.buildBrains(false);}
 restore(){this.buildBrains(true);this.state.request=null;}
 buildBrains(restoring){
  this.actors={citizen:target(301),guard:target(302)};
  if(!this.actors.citizen?.valid||!this.actors.guard?.valid)throw Error('AI Workshop actors are missing');
  const stop=who=>()=>{if(this.actors[who]?.valid)this.actors[who].setKinematicVelocity(zero,zero);};
  const needs={};
  for(const action of ['eat','rest','socialize','wander'])needs[action]={
   start:()=>{this.state.decisions++;},
   update:(dt,context,selection)=>{
    const destination=target(selection.target);
    if(!destination?.valid){this.brains.citizen.finish('failure',context);return;}
    if(!walk(this.actors.citizen,destination,dt))return;
    const key=action==='eat'?'hunger':action==='rest'?'fatigue':'company';
    if(action==='wander'){this.brains.citizen.finish('success',context);return;}
    this.state.services[action]+=dt;
    // Service progresses only while physically at its target. Partial progress
    // survives a deliberate interruption and save; resources change on completion.
    if(this.state.services[action]>=1.2){
     this.state.services[action]=0;
     if(action==='eat'&&this.state.pantry<=0){this.brains.citizen.finish('failure',context);return;}
     this.state[key]=clamp(this.state[key]-.48);
     if(action==='eat'){this.state.pantry--;this.state.meals++;}
     if(action==='rest')this.state.rested++;
     if(action==='socialize')this.state.chats++;
     this.brains.citizen.finish('success',context);
    }
   },
   stop:stop('citizen')
  };
  const guard={
   patrol:{update:(dt,context,selection)=>{if(walk(this.actors.guard,target(selection.target),dt,2))this.brains.guard.finish('success',context);},stop:stop('guard')},
   hide:{start:()=>{this.state.decisions++;},update:(dt,context,selection)=>{if(walk(this.actors.guard,target(selection.target),dt,3.4)){this.state.shelters++;this.brains.guard.finish('success',context);}},stop:stop('guard')},
   recover:{update:(dt,context)=>{this.actors.guard.setKinematicVelocity(zero,zero);this.state.health=clamp(this.state.health+dt*.13);if(this.state.health>.97)this.brains.guard.finish('success',context);},stop:stop('guard')}
  };
  this.brains={citizen:new UtilityAgent(citizenDefinition,needs,'Mara - needs'),guard:new UtilityAgent(guardDefinition,guard,'Eli - self preservation')};
  if(restoring){if(this.state.citizen)this.brains.citizen.restore(this.state.citizen);if(this.state.guard)this.brains.guard.restore(this.state.guard);}
 }
 candidates(who){
  const actor=this.actors[who],p=actor.transform.position;
  const make=(action,id,facts,eligible=true,reason='')=>{
   const goal=target(id);
   return {action,target:String(id),facts:{...facts,distance:goal?.valid?distance(p,goal.transform.position):25},
    eligible:eligible&&!!goal?.valid,reason:!goal?.valid?'Target is no longer in the world':eligible?'':reason};
  };
  if(who==='citizen')return [
   make('eat',101,{hunger:this.state.hunger},this.state.pantry>0,'Canteen pantry is empty'),
   make('eat',102,{hunger:this.state.hunger},this.state.pantry>0,'Canteen pantry is empty'),
   make('rest',103,{fatigue:this.state.fatigue}),make('socialize',104,{company:this.state.company}),
   make('wander',105,{})
  ];
  const covers=[201,202],atCover=covers.some(id=>distance(p,target(id).transform.position)<.16);
  const nearest=covers.slice().sort((a,b)=>distance(p,target(a).transform.position)-distance(p,target(b).transform.position))[0];
  return [
   ...covers.map(id=>make('hide',id,{health:this.state.health},this.state.health<.85&&!atCover,'Safe enough, or already in cover')),
   make('recover',nearest,{health:this.state.health},atCover&&this.state.health<.98,'Recovery requires shelter and an injury'),
   make('patrol',203,{},distance(p,target(203).transform.position)>.24,'Patrol point already reached'),
   make('patrol',204,{},distance(p,target(204).transform.position)>.24,'Patrol point already reached')
  ];
 }
 hurt(){this.state.health=clamp(this.state.health-.42);this.state.damage++;this.state.message='Eli took damage. Watch hide compete against patrol, then recovery.';}
 fixedUpdate(dt){
  if(!this.brains)return;
  if(this.state.paused){for(const actor of Object.values(this.actors))actor.setKinematicVelocity(zero,zero);return;}
  this.state.time+=dt;
  this.state.hunger=clamp(this.state.hunger+dt*.016);
  this.state.fatigue=clamp(this.state.fatigue+dt*.012);
  this.state.company=clamp(this.state.company+dt*.009);
  // The tour only modifies ordinary sensed inputs. It never selects actions.
  if(this.state.watch){
   const stage=Math.floor(this.state.time/12);
   if(stage>this.state.tourStage){this.state.tourStage=stage;
    if(stage%4===1)this.hurt();
    else if(stage%4===2){this.state.fatigue=.95;this.state.message='Mara feels exhausted. The rest action should become more attractive.';}
    else if(stage%4===3){this.state.company=.98;this.state.message='Mara wants company. Compare the social action and its target distance.';}
    else {this.state.hunger=.96;this.state.message='Mara is hungry again. Both canteens compete as separate targets.';}
   }
  }
  for(const who of ['citizen','guard']){
   this.brains[who].tick(dt,()=>this.candidates(who),this);
   this.state[who]=this.brains[who].snapshot();
  }
  const traceSecond=Math.floor(this.state.time/2);
  if(traceSecond>this.state.traceSecond){this.state.traceSecond=traceSecond;console.log('AI_WORKSHOP '+JSON.stringify({time:this.state.time,health:this.state.health,meals:this.state.meals,rested:this.state.rested,chats:this.state.chats,shelters:this.state.shelters,pantry:this.state.pantry,citizen:this.brains.citizen.inspection.selected,guard:this.brains.guard.inspection.selected}));}
 }
 uiUpdate(){
  if(input.pressed('pause'))this.state.paused=!this.state.paused;
  if(input.pressed('autopilot')){this.state.watch=!this.state.watch;this.state.message='Watch tour '+(this.state.watch?'enabled':'disabled')+'. Your controls still work.';}
  if(input.pressed('hurt_guard'))this.hurt();
  if(input.pressed('raise_hunger')){this.state.hunger=.98;this.state.message='Hunger raised. Candidate scores change at the next evaluation.';}
  if(input.pressed('raise_fatigue')){this.state.fatigue=.98;this.state.message='Fatigue raised. Watch commitment and switching advantage.';}
  if(input.pressed('raise_social')){this.state.company=.98;this.state.message='Company need raised.';}
  if(input.pressed('restock')){this.state.pantry=this.state.pantry?0:7;this.state.message=this.state.pantry?'Canteens restocked. Eating is eligible.':'Canteens closed. Eating is ineligible even if hunger is high.';}
  if(!saves.busy){
   if(input.pressed('restart')){scenes.reload();return;}
   if(input.pressed('save_game'))this.state.request=saves.save('little-haven');
   if(input.pressed('load_game'))this.state.request=saves.load('little-haven');
  }
  const hud=ui.get('ai_workshop');if(!hud)return;
  hud.get('title').text='LITTLE HAVEN | Native Utility AI, project-owned actions\n'+
   'H: hunger   T: fatigue   C: company   D: hurt guard   F: close/restock food\n'+
   'A: watch tour '+(this.state.watch?'ON':'OFF')+'   Space: '+(this.state.paused?'resume':'pause')+'   K/L: save/load   R: reset';
  const selected=who=>{const s=this.brains?.[who].inspection?.selected;return s?s.action+' -> '+(targetNames[s.target]??s.target):'choosing...';};
  const request=this.state.request?saves.status(this.state.request):null;
  hud.get('status').text='MARA  |  Hunger '+percent(this.state.hunger)+'  Fatigue '+percent(this.state.fatigue)+'  Company '+percent(this.state.company)+'  |  '+selected('citizen')+'\n'+
   'ELI  |  Health '+percent(this.state.health)+'  |  '+selected('guard')+'  |  Pantry '+this.state.pantry+' meals\n'+
   'Satisfied needs '+(this.state.meals+this.state.rested+this.state.chats)+'  |  Reached shelter '+this.state.shelters+'  |  '+this.state.message+
   (request?'\nSave: '+request.state+(request.error?' - '+request.error:''):'');
 }
 presentationUpdate(){world.setView({position:{x:0,y:0,z:25},rotation:{w:1,x:0,y:0,z:0}},60,{projection:'orthographic',height:17,near:.01,far:100});}
 destroy(){if(this.brains)for(const brain of Object.values(this.brains))brain.dispose();world.clearView();}
}
