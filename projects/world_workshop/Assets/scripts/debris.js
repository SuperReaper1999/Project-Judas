export const properties={label:{type:'string',default:'default'},owner:{type:'entity',default:null}};
// Construction state is input, then ordinary M61 state owns resumed instances.
export default class {
 constructor({entity,properties,initialState,restored}){
  this.entity=entity;this.props=properties;
  this.state={label:properties.label,payload:initialState,restored,constructedVelocity:entity.velocity,started:0};
 }
 start(){this.state.started++;}
 restore(){this.state.restored=true;}
}
