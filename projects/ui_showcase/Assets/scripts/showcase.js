import {ui,input,players,saves,world} from 'judas';
import {layoutIds} from './layout.js';
const PLAYER_UI='4143c08786d02effe162f805acad2bdd';
const pages=['menu','settings','inventory','dialogue','entry','local'];
const names=['Copper cable','Field ration','Survey lens','Battery cell','Medical kit','Weatherproof cloth','Old world token','Navigation chip'];
export default class FieldNotes {
 constructor(){this.state={page:'menu',count:3000,scale:1,dialogue:'MARA / CARTOGRAPHER\n\nYou found the old field station. The maps survived, but half the names are missing.\n\nTake a breath. Choose a route, or leave a note for the next traveller.\n\nThe northern route crosses open ground. The southern route follows the caves. Both lead back here.\n\nEvery good map begins with someone paying attention.',message:'Ready · click a section to explore'};}
 start(){this.bind(false);}
 restore(){this.bind(true);}
 bind(restoring){this.doc=ui.get('showcase');this.locals=[];this.pending=null;this.layouts={};for(const id of layoutIds)this.layouts[id]=this.doc.get(id).layout;ui.debugOverlayVisible=false;input.pointerCapture=false;this.populate(this.state.filter??'');this.doc.get('choice').setItems([{id:'north',text:'Take the northern route'},{id:'south',text:'Explore the cave route'},{id:'stay',text:'Ask about the station'}]);this.doc.get('story').text=this.state.dialogue;this.show(this.state.page);if(restoring&&this.state.ui)this.doc.restoreState(this.state.ui);this.applyScale(this.state.scale);this.applyContrast(this.doc.get('contrast').value>.5);this.status(this.state.message);}
 status(text){this.state.message=text;this.doc.get('status').text=text;}
 show(page){this.state.page=page;for(const name of pages)this.doc.get(name).visible=name===page;this.doc.focus(page==='entry'?'name':page==='inventory'?'filter':'nav_'+page);}
 applyScale(value){this.state.scale=value;this.doc.scale=value;this.reflow(true);}
 reflow(force=false){const size=world.viewport;const portrait=size.width/size.height<1.3;const reference=portrait?{x:720,y:Math.min(4000,720*size.height/Math.max(1,size.width))}:{x:1280,y:720};const identity=`${reference.y}/${portrait}/${this.state.scale}`;if(!force&&this.layoutIdentity===identity)return;this.layoutIdentity=identity;this.doc.referenceSize=reference;this.doc.get('subtitle').text=portrait?'Native text · Live lists · Independent focus':'UI SHOWCASE     /     Native text · Live content · Independent focus';this.doc.get('subtitle').configure({fontSize:portrait?16:18});this.doc.get('status').configure({fontSize:portrait?16:17});const factor=this.state.scale;const navLabels=portrait?['01 Welcome','02 Settings','03 Supplies','04 Dialogue','05 Text','06 Players']:['01   Welcome','02   Settings','03   Inventory','04   Dialogue','05   Text entry','06   Local players'];pages.forEach((page,n)=>{this.doc.get('nav_'+page).text=navLabels[n];});
  const layouts=Object.fromEntries(Object.entries(this.layouts).map(([id,value])=>[id,{offset:{...value.offset},size:{...value.size}}]));
  const set=(id,x,y,w,h)=>{layouts[id]={offset:{x,y},size:{x:w,y:h}};};
  if(portrait){const height=reference.y,contentHeight=Math.max(470,height-430);set('brand',24,24,680,48);set('subtitle',24,78,676,48);set('nav',16,138,688,176);pages.forEach((page,n)=>set('nav_'+page,12+n%3*222,12+Math.floor(n/3)*78,210,62));set('save',16,height-64,100,44);set('load',128,height-64,100,44);set('status',248,height-62,454,58);
   for(const page of pages){set(page,16,334,688,contentHeight);set(page+'_title',24,18,640,72);set(page+'_subtitle',24,94,640,65);}set('welcome',24,180,630,200);set('explore',24,405,600,60);set('keys',24,490,630,90);
   set('scale_label',24,180,620,36);set('scale',24,232,620,38);set('contrast',24,310,620,52);set('volume_label',24,400,620,40);set('volume',24,450,620,32);
   set('filter',24,174,398,50);set('add',444,174,220,50);set('items',24,248,354,Math.min(500,contentHeight-280));set('detail',404,254,252,330);
   set('conversation',24,174,380,Math.min(500,contentHeight-290));set('story',16,12,340,1250);set('choice',428,174,236,320);set('reply',24,contentHeight-80,640,50);set('dialogue_hint',428,520,232,130);
   set('name_label',24,174,620,32);set('name',24,214,640,54);set('note_label',24,290,620,32);set('note',24,330,640,54);set('readonly',24,420,640,50);set('entry_hint',24,510,640,210);set('start_local',24,174,640,60);set('local_help',24,275,640,250);
  }
  for(const [id,layout] of Object.entries(layouts))if(id!=='canvas')this.doc.get(id).setLayout({offset:{x:layout.offset.x/factor,y:layout.offset.y/factor},size:{x:layout.size.x/factor,y:layout.size.y/factor}});
 }
 applyContrast(bright){for(const id of layoutIds)this.doc.get(id).configure({focusColor:bright?{r:1,g:1,b:1,a:1}:{r:.98,g:.73,b:.29,a:1}});}
 populate(filter){this.visibleItems=[];const query=filter.toLocaleLowerCase();for(let n=0;n<this.state.count;n++){const text=`${names[n%names.length]} · ${String(n+1).padStart(4,'0')}`;if(text.toLocaleLowerCase().includes(query))this.visibleItems.push({id:'supply-'+n,text,enabled:n%73!==72});}this.doc.get('items').setItems(this.visibleItems);this.status(`${this.visibleItems.length} supplies · stable keyed rows · scroll or use arrows`);}
 uiUpdate(){this.reflow();if(input.pressed('home')&&this.locals.length)this.closeLocal();if(input.pressed('save'))this.save();if(input.pressed('load'))this.load();if(this.pending){const result=saves.status(this.pending);if(result.state==='completed'||result.state==='failed'){this.status(`Save service: ${result.state}${result.error?' / '+result.error:''}`);this.pending=null;}}}
 save(){if(this.locals.length){this.closeLocal();}this.state.ui=this.doc.captureState();this.pending=saves.save('field-notes',{name:'Field Notes'});this.status('Saving interface and project state…');}
 load(){if(this.locals.length)this.closeLocal();this.pending=saves.load('field-notes');this.status('Loading Field Notes…');}
 onUI(event){
  if(event.document.startsWith('desk-')){const entry=this.locals.find(x=>x.docName===event.document);if(entry&&(event.type==='change'||event.type==='submit'))entry.doc.get('status').text=event.item?`Selected: ${event.text}`:`Callsign: ${event.text}`;return;}
  if(event.document!=='showcase')return;
  if(event.type==='click'){
   if(event.element.startsWith('nav_'))this.show(event.element.slice(4));
   if(event.element==='explore')this.show('inventory');
   if(event.element==='save')this.save();if(event.element==='load')this.load();
   if(event.element==='add'){this.state.count=Math.min(10000,this.state.count+12);this.populate(this.doc.get('filter').text);}
   if(event.element==='start_local')this.openLocal();
  }
  if(event.type==='change'){
   if(event.element==='filter'){this.state.filter=event.text;this.populate(event.text);}
   if(event.element==='items')this.doc.get('detail').text=`${event.text}\n\nStable key: ${event.item}\n\nThis is project inventory data. The engine supplies selection and scrolling.`;
   if(event.element==='scale')this.applyScale(this.doc.get('scale').value);
   if(event.element==='contrast'){this.applyContrast(this.doc.get('contrast').value>.5);}
   if(event.element==='volume')this.status(`Project audio preference: ${Math.round(event.value*100)}%`);
   if(event.element==='choice'){const replies={north:'Take water. The ridge offers a clear view of the coast.',south:'Bring a lamp. The caves remember every footstep.',stay:'We kept the station open because someone always comes back.'};this.state.dialogue+='\n\nYOU / '+event.text+'\n\nMARA / '+replies[event.item];this.doc.get('story').text=this.state.dialogue.slice(-3000);this.status('Dialogue choice recorded in project state');}
  }
  if(event.type==='submit'){if(event.element==='reply'){this.state.dialogue+='\n\nYOU / '+event.text+'\n\nMARA / I will mark that in the field notes.';this.doc.get('story').text=this.state.dialogue.slice(-3000);this.doc.get('reply').text='';}else this.status('Submitted: '+event.text);}
 }
 openLocal(){this.doc.visible=false;const map=(second)=>['ui_up','ui_down','ui_activate','ui_next','ui_click','ui_scroll'].map((name,n)=>({name,axis:name==='ui_scroll',bindings:[{control:second?['key:W','key:S','key:Space','key:Tab','mouse:Left','mouse:wheelY'][n]:['key:Up','key:Down','key:Return','key:Tab','mouse:Left','mouse:wheelY'][n],scale:1,deadzone:0},...(n<3?[{control:['pad:DpadUp','pad:DpadDown','pad:South'][n],scale:1,deadzone:0}]:[])]}));
  for(let n=0;n<2;n++){const player=players.create('Desk '+(n+1),map(n===1));player.viewport={x:n*.5,y:0,width:.5,height:1};player.assign('keyboard',{controls:n?['key:W','key:S','key:Space']:['key:Up','key:Down','key:Return','key:Tab']});player.assign('touch',{region:{x:n*.5,y:0,width:.5,height:1}});if(n===0)player.assign('mouse');else{const pad=players.devices.find(d=>d.connected&&d.kind==='gamepad');if(pad)player.assign(pad.id);}
   const docName='desk-'+player.id,doc=ui.load(PLAYER_UI,docName);doc.player=player;doc.get('title').text='FIELD DESK / PLAYER '+(n+1);doc.get('items').setItems(Array.from({length:100},(_,i)=>({id:'supply-'+i,text:names[i%names.length]+' / '+(i+1)})));doc.focus('items');this.locals.push({player,doc,docName});}
 }
 closeLocal(){for(const entry of this.locals)if(entry.player.valid)entry.player.leave();this.locals=[];this.doc.visible=true;this.show('local');}
 destroy(){for(const entry of this.locals??[])if(entry.player.valid)entry.player.leave();}
}
