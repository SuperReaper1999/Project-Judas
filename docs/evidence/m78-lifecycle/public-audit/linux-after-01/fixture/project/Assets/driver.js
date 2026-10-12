
import {world,session,saves} from 'judas';
const asset='10045fdd36f5c24355a48f7a4bb5abf9';
export default class {
  constructor({restored}) {
    this.restoredConstruction=restored;this.pending=0;
    this.state={started:0,restores:0,updates:0,score:37,spawned:false};
  }
  start(){this.state.started++;session.set('audit.marker','saved-marker');}
  restore(){
    this.state.restores++;this.pending=0;
    this.state.restoreSawRestored=this.restoredConstruction;
    const recovered=saves.resolve(this.state.sentinelReference);
    this.state.resolvedReference=recovered?.id??null;
    this.state.resolvedValid=recovered?.valid===true;
  }
  update(){
    this.state.updates++;
    if(this.pending)this.state.lastStatus=saves.status(this.pending);
    const command=session.get('audit.command');
    if(!command)return;session.delete('audit.command');
    if(command==='spawn') {
      const spawn=(serial,x)=>world.spawnPrefab(asset,{position:{x,y:4,z:0}},{
        velocity:{x:serial,y:0,z:0},angularVelocity:{x:0,y:serial*.25,z:0},
        scripts:[
          {slot:1,properties:{kind:'root',label:`Instance ${serial}`,external:{entity:'3'}},state:{serial,phase:'initial'}},
          {source:'2',slot:1,state:{serial,phase:'child'}}
        ]});
      const a=spawn(1,12),b=spawn(2,20);
      this.state.instances=[{root:a.id,child:a.children[0]?.id??null},{root:b.id,child:b.children[0]?.id??null}];
      try {
        world.spawnPrefab(asset,{}, {scripts:[{slot:1,state:{serial:999}},{source:'2',slot:99,state:{}}]});
        this.state.invalidSlot={typeError:false,message:'accepted'};
      } catch(error) {this.state.invalidSlot={typeError:error instanceof TypeError,message:String(error)};}
      const stale=world.entity('4');stale.destroy();this.state.staleValid=stale.valid;
      this.state.staleReference=saves.reference(stale);
      try {
        world.spawnPrefab(asset,{}, {scripts:[{slot:1,properties:{kind:'root',external:{entity:stale.id}},state:{}}]});
        this.state.invalidReference={typeError:false,message:'accepted'};
      } catch(error) {this.state.invalidReference={typeError:error instanceof TypeError,message:String(error)};}
      this.state.spawned=true;
    } else if(command==='save') {
      this.state.sentinelReference=saves.reference(world.entity('2'));
      this.pending=saves.save('valid',{name:'Public lifecycle audit',metadata:{audit:'M78 continuation'}});
      this.state.request=this.pending;
    } else if(command==='reject') {
      const target=world.entity('2');
      target.transform={position:{x:7,y:9,z:-3},rotation:{w:1,x:0,y:0,z:0}};
      target.velocity={x:-4,y:2,z:3};target.angularVelocity={x:.25,y:-.5,z:.75};
      this.state.score=91;session.set('audit.marker','live-negative-control');
      this.pending=saves.load('invalid');this.state.request=this.pending;
    } else if(command==='load') {
      this.pending=saves.load('valid');this.state.request=this.pending;
    }
  }
}
