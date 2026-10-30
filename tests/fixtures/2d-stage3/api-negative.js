// Normal project module for the Stage 3 application negative-control fixture.
export const properties={sideView:{type:'boolean',default:true}};
export default class {
 constructor({entity}){this.entity=entity;this.state={frames:0,checked:false};}
 fixedUpdate(){
  this.state.frames++;
  const info=this.entity.spriteAnimation;
  if(!info?.ready){if(this.state.frames>100)throw Error('sprite resource never became ready');return;}
  if(this.state.checked)return;
  const assert=(ok,label)=>{if(!ok)throw Error(label);};
  const before=JSON.stringify(this.entity.spriteAnimation);
  let rejects=0;
  for(const patch of [{clip:'missing'},{sheet:'invalid'},{time:NaN},{speed:Infinity},{speed:'1'},{playing:1},{unknown:true}]){
   let threw=false;try{this.entity.setSpriteAnimation(patch);}catch(e){threw=e instanceof TypeError;}
   assert(threw,'bad playback patch must raise catchable TypeError');rejects++;
   assert(JSON.stringify(this.entity.spriteAnimation)===before,'bad playback patch must be atomic');
  }
  info.events.push({name:'injected'});
  assert(this.entity.spriteAnimation.events.length===0,'events are detached observations');
  this.entity.setSpriteAnimation({clip:'run',time:.25,playing:false,speed:-1});
  const sought=this.entity.spriteAnimation;
  assert(sought.time===.25&&sought.clip==='run'&&!sought.playing&&sought.speed===-1&&sought.events.length===0,'partial seek/pause/reverse patch');
  console.log('STAGE3_API_NEGATIVE_PASS rejects='+rejects+' atomic=true detached=true seek=true');
  this.state.checked=true;
 }
}
