import {world,input,ui,scenes,saves,session,physics,profiler} from 'judas';
export const properties={coins:{type:'boolean',default:false}};
const COIN='fa183fe7df1665d7901f3ed5acb46bb2';
const BALL='bc2db04a471a7f77082b762b2ebc79b2'; // Filled from the authored prefab metadata by the project generator.
const v=(x=0,y=0,z=0)=>({x,y,z});
const identity={w:1,x:0,y:0,z:0};
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
export default class {
 constructor({properties}={}){this.coins=!!properties?.coins;this.state={version:1,time:0,started:false,chute:0,bank:60,dropped:0,collected:0,lost:0,score:0,best:0,autoplay:false,won:false,balls:[],lastAward:0};this.queue=[];this.cooldown=0;this.notice='';this.noticeTime=0;this.request=0;}
 start(){this.bind(true);}
 restore(){this.bind(false);}
 bind(initial){this.doc=ui.get('tuppence');this.pause(false);ui.debugOverlayVisible=false;input.pointerCapture=false;world.setView({position:this.coins?v(0,6,8.5):v(0,9,13.4),rotation:{w:Math.cos(-.58/2),x:Math.sin(-.58/2),y:0,z:0}},58,{near:.05,far:70});
  this.doc.get('title').text=this.coins?'TUPPENCE / 2p':'TUPPENCE';this.doc.get('subtitle').text=this.coins?'COIN PUSHER / Real flat coins. Real shelf. Real payout.':'BALL PUSHER / Drop brass. Chase the centre pocket.';
  this.state.best=Math.max(this.state.best,session.get(this.coins?'tuppence-coin-best':'tuppence-best')||0);
  if(initial)this.state.balls=world.queryTags(['arcade_piece']).map(e=>saves.reference(e));
  this.balls=this.state.balls.map(r=>({reference:r,entity:saves.resolve(r)})).filter(x=>x.entity?.valid);this.queue=[];this.cooldown=0;this.notice='A / D to aim. SPACE to drop. Watch the moving shelf.';this.notice=this.coins?'A / D choose slot. SPACE inserts one physical 2p coin.':this.notice;this.noticeTime=8;
 }
 pause(open){this.doc.modal=open;this.doc.get('menu').visible=open;this.doc.get('shade').visible=open;input.pointerCapture=false;}
 uiUpdate(dt){if(input.pressed('pause'))this.pause(!this.doc.modal);this.noticeTime=Math.max(0,this.noticeTime-(dt||.016));const s=this.state;
  this.doc.get('score').text=(this.coins?'PAID OUT ':'SCORE ')+s.score+(this.coins?'p':'');this.doc.get('bank').text=this.coins?s.bank+' COINS':s.bank+' DROPS';this.doc.get('goal').text=(s.won?'JACKPOT CLEARED':this.coins?'TARGET 100p':'TARGET 500')+'  /  BEST '+s.best;
  this.doc.get('auto').text='AUTOPLAY: '+(s.autoplay?'ON':'OFF');this.doc.get('drop').text=this.coins?'INSERT 2p  [SPACE]':'DROP BALL  [SPACE]';
  this.doc.get('drop').enabled=s.bank>0&&this.balls.length<(this.coins?32:208);
  this.doc.get('notice').text=this.noticeTime>0?this.notice:s.bank===0?'Waiting for returns... or start a new machine':'';
  this.doc.get('hint').text=(this.coins?'A / D choose slot. SPACE inserts 2p. Coins over the front edge pay out.  ':'A / D aim the chute. SPACE drops one ball. Centre pocket pays double.  ')+this.balls.length+' pieces in play';
  if(this.request){const r=saves.status(this.request);if(r?.state==='failed'||r?.state==='completed'||r?.state==='committed'){this.notice=r.state==='failed'?'Save/load failed: '+r.error:'Machine '+this.operation+' complete';this.noticeTime=5;this.request=0;}}
 }
 onUI(e){if(e.document!=='tuppence')return;if(e.type==='back'){this.pause(false);return;}if(e.type!=='click')return;
  if(e.element==='pause_button')this.pause(true);else if(e.element==='resume')this.pause(false);else if(e.element==='restart')scenes.reload();else if(e.element==='quit')ui.quit();else if(e.element==='mode_balls'||e.element==='mode_coins')scenes.load(e.element==='mode_coins'?'Scenes/coins.judas':'Scenes/cabinet.judas');
  else if(e.element==='save'||e.element==='load'){if(this.request)return;this.operation=e.element;this.queue.push(e.element);this.pause(false);}
  else if(!this.doc.modal&&['drop','aim_left','aim_right','auto'].includes(e.element))this.queue.push(e.element);
 }
 drop(){if(this.cooldown>0||this.state.bank<=0||this.balls.length>=(this.coins?32:208))return false;
  const pos=v(this.state.chute,this.coins?3.05:5.05,this.coins?-.8:-.5);
  // An occupied nozzle refuses a paid drop; it never creates interpenetrating balls.
  if(physics.sphereCast(pos,.215,v(0,-1,0),0))return false;
  const e=world.spawnPrefab(this.coins?COIN:BALL,{position:pos,rotation:identity});if(!e?.valid)return false;
  this.balls.push({reference:saves.reference(e),entity:e});this.state.bank--;this.state.dropped++;this.state.started=true;this.cooldown=.33;world.entity('90').playAudioOneShot();return true;
 }
 fixedUpdate(dt){if(this.doc.modal)return;const s=this.state;this.cooldown=Math.max(0,this.cooldown-dt);
  for(const command of this.queue.splice(0)){if(command==='drop')this.drop();else if(command==='auto'){s.autoplay=!s.autoplay;s.started=true;}
   else if(command==='aim_left')s.chute=clamp(s.chute-.5,this.coins?-.85:-2.7,this.coins?.85:2.7);else if(command==='aim_right')s.chute=clamp(s.chute+.5,this.coins?-.85:-2.7,this.coins?.85:2.7);
   else if(!this.request&&(command==='save'||command==='load')){this.request=command==='save'?saves.save('tuppence',{name:'Tuppence machine'}):saves.load('tuppence');}}
  s.chute=clamp(s.chute+input.axis('aim')*dt*2.4,this.coins?-.85:-2.7,this.coins?.85:2.7);if(input.pressed('drop'))this.drop();
  if(s.started)s.time+=dt;
  if(s.autoplay){s.chute=Math.sin(s.time*.73)*(this.coins?.8:2.6);if((this.nextAuto||0)<=s.time){if(this.drop())this.nextAuto=s.time+.75;}}
  // Prescribed machinery supplies work; all ball response comes from native contacts.
  const shelf=world.entity('6');
  if(this.coins){const target=-.825+.35*(1-Math.cos(s.time*.6));physics.joint(world.entity('16')).setMotor(-clamp((target-shelf.transform.position.z)*2,-.3,.3),3);}
  else shelf.moveKinematic({position:v(0,1.6,-1.5+.65*(1-Math.cos(s.time*1.65))),rotation:identity});
  world.entity('7').transform={position:v(s.chute,this.coins?3.2:5.3,this.coins?-.8:-.5)};
  const remain=[];
  for(const b of this.balls){const e=b.entity;if(!e?.valid)continue;const p=e.transform.position;
   if(p.y<.95&&p.z>(this.coins?2.035:3.7)&&p.z<(this.coins?3.025:5.5)&&Math.abs(p.x)<(this.coins?2.22:3.42)){const centre=!this.coins&&Math.abs(p.x)<.8,points=this.coins?2:centre?20:10;s.score+=points;s.bank+=centre?2:1;s.collected++;s.lastAward=points;
    s.best=Math.max(s.best,s.score);session.set(this.coins?'tuppence-coin-best':'tuppence-best',s.best);this.notice=(centre?'CENTRE POCKET!  +':'BANKED  +')+points+(this.coins?'p':' points');this.noticeTime=2;world.entity(centre?'92':'91').playAudioOneShot();e.destroy();
    if(!s.won&&s.score>=(this.coins?100:500)){s.won=true;this.notice=this.coins?'100p PAID OUT — keep playing':'JACKPOT! 500 CLEARED — keep playing';this.noticeTime=5;}
   }else if(p.y< -2||Math.abs(p.x)>12||Math.abs(p.z)>15){s.lost++;e.destroy();}else remain.push(b);
  }
  this.balls=remain;s.balls=remain.map(b=>b.reference);profiler.counter('Tuppence physical balls',remain.length,'latest');
 }
 destroy(){input.pointerCapture=false;world.clearView();}
}
