// Use a model cooked by the shared editor/CLI import service. No FBX parsing in JS.
export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}) {this.entity=entity;this.state={ready:false,parts:false,motion:false,missing:false};}
  update() {
    const a=this.entity.animation,parts=this.entity.modelParts;
    if(!a||!a.info.ready||!parts||this.state.ready)return;
    const part=parts[0];
    this.state.parts=!!part&&this.entity.setPartVisible(part.identity,false)&&this.entity.setPartVisible(part.identity,true);
    // Track intervals are in model space. Scripts choose whether/how to turn
    // them into motor intent; sampling does not move any entity or physics body.
    const delta=a.rootMotion('WalkingExtract',.1,.2,true);
    this.state.motion=!!delta&&Number.isFinite(delta.translation.z);
    this.state.missing=a.rootMotion('not-an-authored-clip',0,1)===null;
    this.state.ready=true;
  }
}
