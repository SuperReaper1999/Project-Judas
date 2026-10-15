import {world,input,physics,scenes} from 'judas';
import {missions} from './campaign-core.js';
import {sites} from './planetfall.js';
import {location} from './regions.js';
import {add,sub,mul,length,norm} from './math.js';

// Context selection is shared by the prompt and execution. One E press has one
// owner; queued menu intent executes only after the modal releases fixed steps.
export default class Interaction {
 constructor(p){this.p=p;this.pending=null;this.request=null;this.choice='contain';}
 queue(kind){this.pending=kind;}
 consume(kind){if(this.request!==kind)return false;this.request=null;return true;}
 context(){const p=this.p,pos=p.entity.transform.position;
  if(p.ship?.piloting)return {kind:'ship',label:'Leave chair (release flight controls to stop)'};
  if(p.rover?.active)return {kind:'rover',label:'Leave rover'};
  if(p.grab?.held)return {kind:'grab',label:'Put down carried object'};
  const ship=p.ship;if(ship){const local=ship.local(pos),inside=Math.abs(local.x)<2.15&&local.y>-.2&&local.y<3.2&&Math.abs(local.z)<4.15;
   if(inside&&length(sub(pos,ship.point({x:0,y:.2,z:-1.7})))<2)return {kind:'ship',label:'Sit at command console'};
   if(length(sub(pos,ship.door.transform.position))<3.3)return {kind:'ship',label:(ship.open?'Close':'Open')+' Wayfarer door'};}
  if(p.rover?.entity?.valid&&length(sub(pos,p.rover.entity.transform.position))<3.5)return {kind:'rover',label:'Drive rover'};
  if(world.queryTags(['vendor']).some(e=>e.valid&&length(sub(e.transform.position,pos))<3.6))return {kind:'vendor',label:'Trade supplies'};
  const view=p.camera.pose(),filter={ignored:[p.entity]},sight=physics.raycast(view.position,view.direction,4.5,filter);
  const target=sight?.point||add(view.eye,mul(view.direction,3.2));const hit=physics.raycast(view.eye,norm(sub(target,view.eye)),3.2,filter),e=hit?.entity;
  if(e?.valid){if(e.hasTag('inventory_item'))return {kind:'pickup',label:'Collect '+(e.scriptState(1)?.item||'item').replaceAll('_',' ')};
   if(['24000','24001','24002'].includes(e.id))return {kind:'mine',label:'Mine resource seam'};
   if(e.hasTag('physical')&&e.motionType==='dynamic'&&e.mass<=35&&!e.collider.sensor)return {kind:'grab',label:'Pick up / drag object'};}
  for(let i=0;i<12;i++){const e=world.entity(String(23000+i));if(e?.valid&&length(sub(pos,e.transform.position))<2.7)return {kind:'salvage',label:'Collect salvage'};}
  const m=missions[p.campaign.state.mission];if(m&&m.region===p.campaign.region){const a=sites[m.target].p;if(length(sub(pos,location({x:a[0],y:a[1],z:a[2]})))<(m.target==='ship'?12:6)){
   if(m.kind==='choice')return {kind:'council',label:'Discuss final decision'};
   if(['talk','build','upgrade','recover','rescue'].includes(m.kind))return {kind:'quest',label:({talk:'Talk to ',build:'Build at ',upgrade:'Repair ',recover:'Recover ',rescue:'Rescue '}[m.kind])+sites[m.target].name};}}
  const owner=scenes.resolveRegionEntity('salvage-rover','920'),state=owner?.valid?owner.fracture?.state:null;
  const damaged=state?.interfaces.find(i=>i.plasticWork>0||i.broken&&i.cause==='physical'),part=damaged&&state.parts.find(a=>a.index===damaged.a&&!a.removed);
  if(part?.entity?.valid&&length(sub(pos,part.entity.transform.position))<4&&p.campaign.state.activeJob==='scar')return {kind:'job',label:'Dismantle damaged assembly'};
  if(p.campaign.state.activeJob==='lookout'&&length(sub(pos,location({x:119,y:10.8,z:-80})))<4)return {kind:'job',label:'Record lookout survey'};
  return null;
 }
 tick(){this.request=this.pending;this.pending=null;if(input.pressed('use')){const c=this.context();if(c?.kind==='council'){this.p.hud.page('journal');return;}if(c)this.request=c.kind;}}
}
