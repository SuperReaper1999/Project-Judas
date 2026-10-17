import {network, world, input, ui, physics} from 'judas';
import {v, add, mul, camera} from './math.js';

export const properties = {
 serverAddress:{type:'string',default:'127.0.0.1'},
 port:{type:'number',default:27999}
};
const CHANNELS=['move_x','move_y','interact'];
const CARGO=Array.from({length:12},(_,i)=>String(200+i));
const AVATARS=['100','101'];
const clamp=n=>Math.max(-1,Math.min(1,n));

// All rules, control policy, scoring and camera placement are ordinary project
// JavaScript. Clients send bounded intent; only the host changes physical state.
export default class ConnectedWorkshop {
 constructor({properties}){
  this.props=properties;this.state={delivered:0,blue:0,amber:0,deliveredIds:[]};
  this.records=new Map();this.facing=[v(0,0,-1),v(0,0,-1)];
  this.cooldown=[0,0];this.touchOrigins=new Map();this.message='';
  this.previousMode='';this.queuedShove=false;this.localIntent={};
 }
 start(){this.bind();}
 restore(){this.bind();}
 bind(){
  this.hud=ui.get('connected');ui.debugOverlayVisible=false;input.pointerCapture=false;
  if(this.hud){this.hud.get('address').text=this.props.serverAddress;this.layout();}
  const launch=network.launch;
  this.dedicated=!!launch.dedicated;
  if(launch.mode==='host'&&network.mode==='offline')this.host(launch.port);
  else if(launch.mode==='join'&&network.mode==='offline')this.join(launch.address,launch.port);
  this.refreshMode();
 }
 command(callback){try{callback();this.message='';}catch(error){this.message=String(error);}}
 host(port=this.props.port){this.command(()=>network.host({address:'0.0.0.0',port,maxPeers:4,controls:CHANNELS}));}
 join(address=null,port=this.props.port){
  const server=address||this.hud?.get('address').text||this.props.serverAddress;
  this.command(()=>network.join({address:server.trim(),port,name:'Workshop operator'}));
 }
 leave(){this.command(()=>network.close());}
 onUI(event){
  if(event.document!=='connected'||event.type!=='click')return;
  if(event.element==='host')this.host();
  if(event.element==='join')this.join();
  if(event.element==='leave')this.leave();
  if(event.element==='transfer')this.transfer();
 }
 uiUpdate(){
  this.layout();
  if(input.pressed('host'))this.host();
  if(input.pressed('join'))this.join();
  if(input.pressed('leave'))this.leave();
  if(input.pressed('transfer'))this.transfer();
  this.refreshMode();
  if(!this.hud)return;
  const entry=network.entities.find(e=>e.entity?.id==='100');
  const progress=network.mode==='client'?(entry?.state||{}):this.state;
  this.hud.get('score').text=`${network.mode.toUpperCase()} / BLUE ${progress.blue||0}  |  AMBER ${progress.amber||0}  |  ${(progress.delivered||0)===12?'ALL FREIGHT DELIVERED':(progress.delivered||0)+' OF 12 DELIVERED'}`;
  const peers=network.peers.filter(p=>p.connected),status=network.status;
  this.hud.get('status').text=`${String(status)} / ${peers.length} connected peers / local ${network.peerId||'offline'}\n${this.message||network.error||'Shove the physical crates into either bay. Host owns simulation; remote clients submit intent.'}`;
  this.hud.get('transfer').enabled=network.mode==='host';
 }
 layout(){
  if(!this.hud||this.dedicated)return;
  const portrait=world.viewport.width<world.viewport.height;if(this.portrait===portrait)return;this.portrait=portrait;
  this.hud.referenceSize={x:portrait?720:1280,y:portrait?1280:720};
  const place=(id,x,y,width,height)=>this.hud.get(id).setLayout({offset:{x,y},size:{x:width,y:height}});
  place('header',0,0,0,portrait?140:123);place('title',24,12,portrait?672:900,32);
  place('subtitle',24,49,portrait?672:1150,portrait?44:28);
  place('score',24,portrait?101:80,portrait?672:1100,28);
  place('status',24,portrait?156:137,portrait?672:1190,portrait?110:68);
  if(portrait){
   place('host',24,-236,208,56);place('join',256,-236,208,56);place('leave',488,-236,208,56);
   place('address_label',24,-175,440,25);place('address',24,-150,440,56);place('transfer',488,-150,208,56);
   place('hint',24,-83,672,45);place('touch',24,-38,672,34);
  }else{
   place('host',24,-124,178,46);place('join',214,-124,178,46);place('leave',404,-124,220,46);
   place('address_label',660,-154,570,25);place('address',660,-124,290,46);place('transfer',966,-124,290,46);
   place('hint',24,-64,1220,28);place('touch',24,-32,1220,26);
  }
 }
 refreshMode(){
  const mode=network.mode;if(mode===this.previousMode)return;
  this.previousMode=mode;this.records.clear();this.touchOrigins.clear();
  this.queuedShove=false;this.localIntent={};
  if(mode==='host'){
   for(const id of [...AVATARS,...CARGO]){
    const entity=world.entity(id);
    this.records.set(id,network.replicate(entity,{owner:'host',state:id==='100'?{kind:'operator',...this.state}:{kind:id==='101'?'operator':'cargo',delivered:this.state.deliveredIds.includes(id)}}));
   }
  }
 }
 transfer(){
  if(network.mode!=='host'){this.message='Only the host can transfer authority.';return;}
  const peer=network.peers.find(p=>p.connected&&p.id!=='host');
  const entry=this.records.get('200');
  if(!peer||!entry){this.message='Connect a guest before transferring cargo authority.';return;}
  entry.owner=entry.owner===peer.id?'host':peer.id;
  this.message='Cargo 1 authority: '+entry.owner+'. Its physical simulation still runs on the host.';
 }
 readIntent(){
  let x=input.axis('move_x'),y=input.axis('move_y'),interact=input.held('interact');
  // Portable project touch policy: drag lower-left, touch lower-right to shove.
  // This uses the same named scalar protocol as keyboards and gamepads.
  const live=new Set();
  for(const t of input.touches){
   live.add(t.id);if(!this.touchOrigins.has(t.id))this.touchOrigins.set(t.id,{x:t.x,y:t.y});
   const start=this.touchOrigins.get(t.id);if(start.y<.28||start.y>.72)continue;
   if(start.x<.55){x+=clamp((t.x-start.x)*8);y-=clamp((t.y-start.y)*8);}
   else if(start.x>.72)interact=true;
  }
  for(const id of this.touchOrigins.keys())if(!live.has(id))this.touchOrigins.delete(id);
  const length=Math.hypot(x,y);if(length>1){x/=length;y/=length;}
  return {move_x:clamp(x),move_y:clamp(y),interact:interact?1:0};
 }
 update(){
  this.refreshMode();this.localIntent=this.dedicated?{}:this.readIntent();
  if(network.mode==='client')this.command(()=>network.sendInput(this.localIntent));
 }
 move(slot,controls,peer,dt){
  const actor=world.entity(AVATARS[slot]),motor=actor?.character;if(!motor)return;
  const x=clamp(controls.move_x||0),y=clamp(controls.move_y||0);
  const length=Math.hypot(x,y),desired=v(x*5.5,0,-y*5.5);
  if(length>.08)this.facing[slot]=v(x/length,0,-y/length);
  const up=motor.up,velocity=motor.velocity,vertical=velocity.x*up.x+velocity.y*up.y+velocity.z*up.z;
  motor.velocity=add(desired,mul(up,vertical));
  this.cooldown[slot]=Math.max(0,this.cooldown[slot]-dt);
  if(controls.interact===1&&this.cooldown[slot]===0){
   this.cooldown[slot]=.45;
   const origin=add(actor.transform.position,v(0,-.15,0));
   const hit=physics.raycast(origin,this.facing[slot],3.4,{ignored:AVATARS.map(id=>world.entity(id))});
   if(hit?.entity&&CARGO.includes(hit.entity.id)){
    const record=this.records.get(hit.entity.id);
    // Project policy allows shared host cargo; a delegated crate accepts its
    // owner and the host. Remote peers never submit body transforms/impulses.
    if(!record||record.owner==='host'||record.owner===peer||peer==='host')
     hit.entity.applyImpulseAtPoint(add(mul(this.facing[slot],22),v(0,2,0)),hit.point);
   }
  }
 }
 fixedUpdate(dt){
  this.refreshMode();if(network.mode==='client')return;
  const remote=network.mode==='host'?network.peers.find(p=>p.connected&&p.id!=='host'):null;
  if(remote?.player)remote.player.entity=world.entity('101');
  const guest=this.records.get('101');if(guest)guest.owner=remote?.id||'host';
  this.move(0,this.localIntent,'host',dt);
  this.move(1,remote?network.input(remote.id):{},remote?.id||'host',dt);
  for(const id of CARGO){
   const entry=this.records.get(id),entity=world.entity(id);
   if(!entity?.valid)continue;
   if(entry&&entry.owner!=='host'&&!network.peers.some(p=>p.connected&&p.id===entry.owner))entry.owner='host';
   // Offline practice uses the same physical outcome and rules.
   if((entry?.state.delivered)||this.state.deliveredIds.includes(id))continue;
   const p=entity.transform.position;
   if(Math.abs(p.x)>9&&Math.abs(p.z)<3.6&&p.y<1.5){
    if(entry)entry.state={kind:'cargo',delivered:true,bay:p.x<0?'blue':'amber'};
    this.state.deliveredIds.push(id);
    this.state.delivered++;this.state[p.x<0?'blue':'amber']++;
   }
  }
  const score=this.records.get('100');if(score)score.state={kind:'operator',...this.state};
 }
 presentationUpdate(){
  if(this.dedicated)return;
  const aspect=world.viewport.width/Math.max(1,world.viewport.height),height=Math.max(21,22/aspect);
  world.setView(camera(v(0,height,height*1.06),v(0,0,.3)),57,{near:.1,far:130});
 }
 destroy(){input.pointerCapture=false;world.clearView();}
}
