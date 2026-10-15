import {location,frame} from './regions.js';
import {world,input,saves,scenes,navigation} from 'judas';
import {add,sub,length,dot} from './math.js';
import {game} from './game.js';
const jobs=[
 {id:'scar',region:'earth',title:'Quill: Scarred Bearings',kind:'damage',p:{x:145,y:0,z:65},reward:{scrap:20,fuel:2,credits:200},brief:'Physically damage the exposed front assembly, approach the damaged section and E to dismantle it for salvage.'},
 {id:'bridge',region:'earth',title:'Rook: The Long Way Home',kind:'drive',p:{x:-65,y:2.5,z:70},reward:{iron:5,credits:100},brief:'Drive the rover to the load-bearing road bridge. Cross its raised deck from west to east.'},
 {id:'lookout',region:'earth',title:'Vale: A Better View',kind:'recover',p:{x:119,y:10.8,z:-80},reward:{aether:2,credits:150},brief:'Climb the research scaffold and reach the roof lookout.'},
 {id:'lunar',region:'moon',title:'Quill: Salvage Rights',kind:'fight',p:{x:-60,y:0,z:60},reward:{alloy:5,fuel:2,credits:150},brief:'Clear three armed scavengers from the salvage depot.'},
 {id:'red',region:'mars',title:'Mara: A Quiet Landing',kind:'fight',p:{x:80,y:0,z:-90},reward:{fuel:3,credits:200},brief:'Clear four infected from the landing approach.'}
];
export default class Jobs {
 constructor(player){this.p=player;this.s=player.campaign.state;this.s.jobs=this.s.jobs||{};this.selected=jobs.findIndex(j=>j.id===this.s.activeJob);for(const r of Object.values(this.s.jobs))if(!r.done)r.refs=r.refs.filter(ref=>saves.resolve(ref)?.valid);}
 tick(dt){const region=this.p.campaign.region;if((input.pressed('side_quest')||this.p.interaction?.consume('contract'))){this.selected=(this.selected+1)%jobs.length;const j=jobs[this.selected];this.s.activeJob=j.id;this.p.campaign.dialogue=j.title+' / '+j.region+' / '+j.brief;}
  const j=jobs.find(j=>j.id===this.s.activeJob);if(!j||j.region!==region)return;const record=this.s.jobs[j.id]||(this.s.jobs[j.id]={done:false,refs:[],defeated:0});if(record.done)return;
  const near=length(sub(this.p.entity.transform.position,location(j.p)))< (j.kind==='drive'?10:4);
  if(j.kind==='damage'){const owner=scenes.resolveRegionEntity('salvage-rover','920');const state=owner?.valid?owner.fracture?.state:null;if(record.pendingPart!==undefined&&state?.parts[record.pendingPart]?.removed)this.finish(j,record);else if((input.pressed('quest_use')||this.p.interaction?.consume('job'))){const damaged=state?.interfaces.find(i=>i.plasticWork>0||i.broken&&i.cause==='physical'),part=damaged&&state.parts.find(p=>p.index===damaged.a&&!p.removed);if(part?.entity?.valid&&length(sub(this.p.entity.transform.position,part.entity.transform.position))<4){if(owner.fracture.remove(part.index,state.revision))record.pendingPart=part.index;}else this.p.campaign.dialogue='Damage an exposed section physically, then approach it to dismantle it.';}}
  if(j.kind==='fight'&&length(sub(this.p.entity.transform.position,location(j.p)))<25){
   if(record.refs.length+(record.defeated||0)<(j.id==='lunar'?3:4)){for(let i=record.refs.length+(record.defeated||0);i<(j.id==='lunar'?3:4);i++){const sample=navigation.sample(location(add(j.p,{x:i*3,y:.1,z:0})),5);if(!sample)continue;const e=world.spawnPrefab(j.id==='lunar'?this.p.props.pirate:this.p.props.zombie,{position:sample.position,rotation:frame()},{scripts:[{slot:1,properties:{kind:j.id==='lunar'?'soldier':'zombie',island:true,variant:'runner'}}]});record.refs.push(saves.reference(e));}}
   record.refs=record.refs.filter(ref=>{const e=saves.resolve(ref);if(e?.valid&&e.scriptState(1)?.dead){record.defeated=(record.defeated||0)+1;return false;}return !!e?.valid;});if(record.defeated>=(j.id==='lunar'?3:4))this.finish(j,record);
  }else if(j.kind==='drive'&&this.p.rover?.active){const d=sub(this.p.entity.transform.position,location(j.p)),x=dot(d,location({x:1,y:0,z:0})),z=dot(d,location({x:0,y:0,z:1})),height=dot(d,location({x:0,y:1,z:0}));if(Math.abs(z)<2){if(x<-4.5)record.approach=true;if(record.approach&&Math.abs(x)<4&&height>0)record.crossed=true;if(record.crossed&&x>6)this.finish(j,record);}}else if(near&&j.kind==='recover'&&(input.pressed('quest_use')||this.p.interaction?.consume('job')))this.finish(j,record);
 }
 finish(j,record){record.done=true;for(const[k,v]of Object.entries(j.reward))this.s.resources[k]=(this.s.resources[k]||0)+v;game.points+=j.reward.credits||0;this.p.campaign.dialogue='SIDE JOB COMPLETE / '+j.title;this.p.campaign.publish();}
}
