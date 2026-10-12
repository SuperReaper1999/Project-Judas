
import {world} from 'judas';
export const properties = {
  kind: {type:'string', default:'child'},
  label: {type:'string', default:'authored'},
  external: {type:'entity', default:null},
  internal: {type:'entity', default:null}
};
export default class {
  constructor({entity,properties,initialState,restored}) {
    this.entity=entity; this.properties=properties;
    this.restoredConstruction=restored; this.initialWasNull=initialState===null;
    this.state={kind:properties.kind,label:properties.label,payload:initialState,
      parent:entity.parent?.id??null,external:properties.external?.id??null,
      internal:properties.internal?.id??null,started:0,restores:0,updates:0};
    if(properties.kind==='root') {
      this.state.constructedVelocity=entity.velocity;
      this.state.constructedAngularVelocity=entity.angularVelocity;
    }
  }
  start(){this.state.started++;}
  restore(){
    this.state.restores++;
    this.state.restoreSawRestored=this.restoredConstruction;
    this.state.restoreInitialNull=this.initialWasNull;
    this.state.restoredParent=this.entity.parent?.id??null;
    this.state.restoredExternal=this.properties.external?.valid===true;
    this.state.restoredInternal=this.properties.internal?.valid===true;
  }
  update(){this.state.updates++;}
}
