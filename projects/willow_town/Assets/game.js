// Presentation, player intent and saves. Decisions and effects live in town.js.
import {entity,input,world,ui,saves} from 'judas';
import Town,{createState} from './town.js';

const percent=v=>Math.round(Math.max(0,Math.min(1,v))*100)+'%';
export const properties={startSpeed:{type:'number',default:1},observationRun:{type:'boolean',default:false}};

const actionNames={jobsearch:'Looking for work',work:'Working',eat:'Buying a meal',rest:'Resting at home',socialize:'Meeting a neighbour',wander:'Taking a stroll',idle:'Taking a breather'};
const colour=(r,g,b,a=1)=>({r,g,b,a});

export default class WillowTown {
 constructor(context={}){
  this.state={...createState(),selected:'301',request:null,lastPick:null,view:null};
  this.state.speed=context.properties?.startSpeed===3?3:1;
  this.observationRun=!!context.properties?.observationRun;
  this.watchFinishing=false;this.watchRequest=null;
  this.town=null;this.doc=null;this.size='';this.traceSecond=0;
 }
 start(){this.open(false);}
 restore(){this.state.request=null;this.open(true);}
 open(restoring){
  this.town=new Town(this.state);this.town.start(restoring);
  this.doc=ui.get('willow_town');
  if(!this.doc)throw Error('Willow Town interface is missing from the scene');
  input.pointerCapture=false;
  this.layout();this.camera();this.paint();
  if(this.observationRun)this.observe('WILLOW_WATCH_START');
 }
 selected(){return this.town?.resident(this.state.selected)??this.state.residents[0];}
 choose(id){
  const person=this.town.resident(String(id));if(!person)return;
  this.state.selected=person.id;
  this.state.message=person.name+' — '+person.job+'. Their decisions and needs are on the right.';
 }
 control(){
  const id=this.state.selected;
  if(this.state.controlled===id){this.town.setControlled(null);return;}
  this.town.setControlled(id);
 }
 onUI(event){
  if(event.document!=='willow_town'||event.type!=='click')return;
  const id=event.element;
  if(id.startsWith('resident-'))this.choose(id.slice(9));
  else if(id==='control')this.control();
  else if(id==='pause')this.state.paused=!this.state.paused;
  else if(id==='speed')this.state.speed=this.state.speed===1?3:1;
  else if(id==='hungry')this.town.setNeed(this.state.selected,'hunger',.98);
  else if(id==='tired')this.town.setNeed(this.state.selected,'fatigue',.98);
  else if(id==='lonely')this.town.setNeed(this.state.selected,'social',.98);
  else if(id==='hurt')this.town.hurt(this.state.selected,.45);
  else if(id==='dismiss')this.town.dismiss(this.state.selected);
  else if(id==='hiring-toggle'){
   this.state.hiringOpen=!this.state.hiringOpen;
   this.state.message=this.state.hiringOpen?'Applications are open. Residents can compete for real vacancies.':'Hiring paused. Existing employees keep their jobs.';
  }else if(id==='job-toggle'){
   const person=this.selected(),job=this.town.job(person.jobId??person.lastJobId);
   if(job)this.town.setJobOpen(job.id,!job.open);
  }
  else if(id==='shop-toggle'){
   this.state.shop.open=!this.state.shop.open;
   this.state.message=this.state.shop.open?'The grocer is open again.':'Grocer closed: meals must become unavailable, however hungry someone is.';
  }else if(id==='restock'){
   this.state.shop.stock+=8;this.state.message='Eight meals added to the shop pantry.';
  }else if(id==='save'||id==='load')this.save(id);
  this.paint();
 }
 save(kind){
  if(saves.busy){this.state.message='Finishing the current save or load first.';return;}
  this.state.request=kind==='save'?saves.save('willow-town'):saves.load('willow-town');
 }
 uiUpdate(){
  if(!this.town)return;
  this.layout();
  if(input.pressed('pause'))this.state.paused=!this.state.paused;
  if(input.pressed('release'))this.town.setControlled(null);
  if(input.pressed('save_game'))this.save('save');
  if(input.pressed('load_game'))this.save('load');
  if(input.pressed('finish_watch')){
   this.state.paused=true;this.watchFinishing=true;this.watchRequest=null;
   this.observe('WILLOW_WATCH_FINAL');
  }
  if(this.watchFinishing&&!this.watchRequest&&!saves.busy){
   this.watchRequest=saves.save('willow-observation');this.state.request=this.watchRequest;
  }
  if(this.watchRequest){
   const status=saves.status(this.watchRequest);
   if(status&&(status.state==='completed'||status.state==='failed')){
    console.log('WILLOW_WATCH_SAVE '+JSON.stringify({state:status.state,error:status.error??'',time:this.state.time}));
    this.watchFinishing=false;this.watchRequest=null;
   }
  }
  this.paint();
 }
 update(){
  // UI consumes its own physical bindings first. Empty scenery never changes
  // selection; a normal short click still works without a fixed step that frame.
  if(!this.town||!input.pressed('select'))return;
  const p=input.pointer,v=world.viewport;
  if(p.x*v.width>=v.width-this.panel||p.y*v.height<66||p.y*v.height>v.height-89)return;
  const ray=world.unproject({x:p.x,y:p.y});
  if(!ray||Math.abs(ray.direction.z)<1e-8)return;
  const t=-ray.origin.z/ray.direction.z;
  const point={x:ray.origin.x+t*ray.direction.x,y:ray.origin.y+t*ray.direction.y};
  let closest=null,best=.88;
  for(const resident of this.state.residents){
   const actor=entity(resident.id);if(!actor?.valid)continue;
   const a=actor.transform.position;
   // Sprite feet are the physical origin; the clickable figure extends upward.
   const d=Math.hypot(a.x-point.x,a.y+.35-point.y);
   if(d<best){best=d;closest=resident;}
  }
  if(closest){this.choose(closest.id);this.state.lastPick={id:closest.id,x:point.x,y:point.y};}
 }
 fixedUpdate(dt){
  if(!this.town)return;
  this.town.step(dt);
  if(this.state.controlled&&!this.state.paused)
   this.town.manualMove({x:input.axis('move_x'),y:input.axis('move_y')},dt);
  // Small periodic observations for reproducible app tests; these reflect native
  // decisions and effects, not a hidden watch-tour choosing actions for them.
  const second=Math.floor(this.state.time/10);
  if(second>this.traceSecond){this.traceSecond=second;
   this.observe('WILLOW_TOWN');
  }
 }
 observe(label){
  const s=this.state;
  console.log(label+' '+JSON.stringify({version:s.version,time:s.time,speed:s.speed,paused:s.paused,
   stock:s.shop.stock,sales:s.shop.sales,controlled:s.controlled,census:s.census,hiringOpen:s.hiringOpen,
   jobs:s.jobs.map(j=>({id:j.id,employeeId:j.employeeId,open:j.open,tasks:j.completedTasks})),
   residents:s.residents.map(r=>{const a=entity(r.id),brain=this.town.inspect(r.id);return {
    id:r.id,name:r.name,alive:r.alive,ageDays:r.ageDays,lifespanDays:r.lifespanDays,
    health:r.health,hunger:r.hunger,fatigue:r.fatigue,social:r.social,job:r.job,jobId:r.jobId,
    jobsLost:r.jobsLost,jobsFound:r.jobsFound,absence:r.absenceWorkingSeconds,
    deathCause:r.deathCause,deathTime:r.deathTime,work:r.workDone,meals:r.meals,rests:r.rests,
    chats:r.chats,money:r.money,position:a?.valid?a.transform.position:null,
    selected:brain?.selected??null,decisionElapsed:brain?.elapsed??null};})}));
 }
 layout(){
  const viewport=world.viewport,w=Math.max(640,viewport.width),h=Math.max(480,viewport.height);
  const key=w+'x'+h;if(key===this.size||!this.doc)return;this.size=key;
  this.panel=Math.max(300,Math.min(370,w*.27));const x=w-this.panel,pad=18,inner=this.panel-pad*2;
  this.doc.referenceSize={x:w,y:h};
  const set=(id,x,y,width,height)=>this.doc.get(id).setLayout({offset:{x,y},size:{x:width,y:height}});
  set('canvas',0,0,w,h);set('header',0,0,w,66);set('sidebar',x,66,this.panel,h-96);
  set('brand',22,9,280,30);set('subtitle',23,39,390,20);
  set('clock',Math.max(350,w*.36),20,220,28);
  for(const [i,id] of ['pause','speed','save','load'].entries())set(id,w-406+i*98,16,90,34);
  set('hiring-toggle',w-584,16,164,34);
  const sy=Math.min(1,(h-112)/660);this.sideScale=sy;
  const line=(id,y,height=25)=>set(id,x+pad,76+y*sy,inner,height*sy);
  line('person-name',0,34);line('person-job',34,22);line('person-life',57,21);
  line('person-action',85,29);line('person-reason',115,37);
  for(const [i,need] of ['hunger','fatigue','social'].entries()){
   line('need-'+need,157+i*30,21);line('track-'+need,180+i*30,6);line('bar-'+need,180+i*30,6);
  }
  line('person-money',256,23);line('control',286,33);line('person-employment',328,33);
  for(const [i,id] of ['dismiss','job-toggle'].entries())set(id,x+pad+i*(inner+8)/2,76+369*sy,(inner-8)/2,29*sy);
  line('scores-title',410,20);line('scores',436,110);line('experiment-label',550,18);
  for(const [i,id] of ['hungry','tired','lonely','hurt'].entries())set(id,x+pad+i*(inner+6)/4,76+573*sy,(inner-18)/4,28*sy);
  line('shop-status',610,20);
  for(const [i,id] of ['shop-toggle','restock'].entries())set(id,x+pad+i*(inner+8)/2,76+633*sy,(inner-8)/2,27*sy);
  const townWidth=w-this.panel;
  for(const [i,r] of this.state.residents.entries())set('resident-'+r.id,16+i*(townWidth-32)/6,h-79,(townWidth-48)/6,35);
  set('town-help',20,h-108,townWidth-40,20);
  set('footer',0,h-30,w,30);set('message',20,h-26,w-40,22);
 }
 camera(){
  const v=world.viewport,w=Math.max(640,v.width),h=Math.max(480,v.height),panel=this.panel??340;
  const height=Math.max(25.5,33*h/Math.max(300,w-panel-24),23*h/Math.max(260,h-180));
  const position={x:panel*.5*height/h,y:-23*height/h,z:30};
  world.setView({position,rotation:{w:1,x:0,y:0,z:0}},60,{projection:'orthographic',height,near:.01,far:100});
  this.state.view={position,height};
 }
 paint(){
  if(!this.doc||!this.town)return;
  const s=this.state,person=this.selected();if(!person)return;
  const get=id=>this.doc.get(id),info=this.town.inspect(person.id),choice=info?.selected;
  get('clock').text='Day '+(1+Math.floor((144+s.time)/432))+'  ·  '+
   String(Math.floor((8+s.time/18)%24)).padStart(2,'0')+':'+String(Math.floor(s.time*60/18)%60).padStart(2,'0');
  get('pause').text=s.paused?'Resume':'Pause';get('speed').text='Speed: '+s.speed+'x';
  get('person-name').text=person.name;get('person-job').text=person.alive?person.job:'Deceased';
  get('person-life').text='Health '+percent(person.health)+'  ·  Age '+person.ageDays.toFixed(1)+' / '+person.lifespanDays+' days';
  get('subtitle').text=s.residents.filter(r=>r.alive).length+' living  ·  '+s.census.deaths+' deaths  ·  '+s.census.hires+' hires  ·  '+s.census.firings+' dismissals';
  get('hiring-toggle').text=s.hiringOpen?'Hiring: open':'Hiring: paused';
  const job=this.town.job(person.jobId??person.lastJobId);
  get('person-employment').text=person.alive?'Jobs found '+person.jobsFound+'  ·  Jobs lost '+person.jobsLost+'\n'+
   (person.jobId?'Absence: '+Math.floor(person.absenceWorkingSeconds)+' / 65 working sec':person.jobLostReason?.startsWith('Missed')?'Missed too much of the workday':person.jobLostReason==='Workplace closed'?'Their previous workplace closed':'Looking for an open vacancy'):
   'Died: '+person.deathCause+'\nEmployment and decisions have ended.';
  get('dismiss').enabled=!!person.alive&&!!person.jobId;
  get('job-toggle').enabled=!!job;get('job-toggle').text=job?.open?'Close workplace':'Open workplace';
  get('control').enabled=!!person.alive;
  for(const id of ['hungry','tired','lonely','hurt'])get(id).enabled=!!person.alive;
  const actor=entity(person.id),destination=choice?entity(choice.action==='jobsearch'?'1105':choice.target):null;
  const travelling=actor?.valid&&destination?.valid&&Math.hypot(actor.transform.position.x-destination.transform.position.x,actor.transform.position.y-destination.transform.position.y)>(choice.action==='socialize'?.9:.25);
  const journey={jobsearch:'Going to the jobs board',work:'Going to work',eat:'Walking to the grocer',rest:'Heading home',socialize:'Meeting a neighbour',wander:'Taking a stroll'};
  get('person-action').text=!person.alive?'Died from '+person.deathCause.toLowerCase():s.controlled===person.id?'You are in control':((travelling?journey[choice?.action]:actionNames[choice?.action])??'Choosing their next task');
  get('person-reason').text=!person.alive?'Their history and stats remain here. Their old job is now available.':s.controlled===person.id?'WASD / arrows to walk. Return to AI or press Esc when you are done.':
   (info?.commitmentRemaining>.05?'Sticking with this task for '+info.commitmentRemaining.toFixed(1)+' seconds.':
    info?.reason==='highest eligible utility'?'This currently has the highest eligible score.':info?.reason==='switch advantage not reached'?"Another choice isn't enough better to interrupt this one.":info?.reason??'Waiting for the next decision.');
  const inner=this.panel-36;
  for(const [need,label] of [['hunger','Hunger'],['fatigue','Tiredness'],['social','Needs company']]){
   get('need-'+need).text=label+'  '+percent(person[need]);
   get('bar-'+need).setLayout({size:{x:inner*Math.max(.005,person[need]),y:6*(this.sideScale??1)}});
  }
  get('person-money').text='Wallet '+Math.floor(person.money)+' coins  ·  Jobs done '+person.workDone;
  get('control').text=s.controlled===person.id?'Return to AI [Esc]':'Take control [WASD]';
  const rows=info?.candidates??[],best=new Map();
  for(const row of rows){const current=best.get(row.action);if(!current||row.score>current.score||(row.eligible&&!current.eligible))best.set(row.action,row);}
  const scoreRows=Array.from(best.values()).sort((a,b)=>Number(b.eligible)-Number(a.eligible)||b.score-a.score);
  get('scores').text=!person.alive?'No active brain or resource claims. A deceased resident cannot be controlled or hired.':s.controlled===person.id?'AI is paused for this person. Everyone else is still making their own decisions.':
   scoreRows.map(row=>(row.eligible?'• ':'× ')+({jobsearch:'Find work',work:'Work',eat:'Meal',rest:'Rest',socialize:'Company',wander:'Stroll',idle:'Breather'}[row.action]??row.action)+'  '+
    (row.eligible?row.score.toFixed(2):'unavailable')).slice(0,6).join('\n');
  get('shop-status').text='Grocer '+(s.shop.open?'open':'closed')+'  ·  '+s.shop.stock+' meals  ·  '+s.shop.sales+' sold';
  get('shop-toggle').text=s.shop.open?'Close shop':'Open shop';
  for(const r of s.residents){get('resident-'+r.id).text=r.name+(r.alive?'':' †');get('resident-'+r.id).configure({background:r.id===person.id?colour(.29,.40,.29):colour(.09,.18,.16,.96)});}
  const status=s.request?saves.status(s.request):null;
  get('message').text=status&&(status.state==='failed'||status.state==='completed')?
   (status.state==='failed'?'Save/load failed: '+status.error:'Saved progress / loaded progress successfully.'):
   s.message??'Click a resident to follow their choices.';
 }
 presentationUpdate(){
  if(!this.town)return;this.camera();
  const selected=this.selected(),actor=selected?entity(selected.id):null,ring=entity('500');
  if(actor?.valid&&ring?.valid){const p=actor.presentedTransform.position;ring.transform={position:{x:p.x,y:p.y,z:0}};
   const point=world.project({x:p.x,y:p.y+(selected.alive?1.15:.6),z:0});
   const label=this.doc.get('selected-label');label.visible=!!point?.inside;
   if(point?.inside){label.text=selected.name+(selected.alive?'':' †');label.setLayout({offset:{x:point.x*world.viewport.width-45,y:point.y*world.viewport.height-14},size:{x:90,y:21}});}
  }
  for(const r of this.state.residents){const a=entity(r.id);if(!a?.valid)continue;
   const v=a.velocity,moving=Math.hypot(v.x,v.y)>.08;
   const frame=moving?Math.floor(this.state.time*7)%4:0;
   a.setSprite({rect:{x:r.alive?frame*.25:0,y:0,width:.25,height:1},
    size:{x:r.alive ? .96 : 1.2,y:r.alive ? 1.28 : .5},pivot:{x:.5,y:.2},
    tint:r.alive?{x:1,y:1,z:1,w:1}:{x:.55,y:.58,z:.54,w:.85},
    flipX:moving?v.x<0:a.sprite.flipX});
  }
 }
 destroy(){this.town?.dispose();world.clearView();}
}
