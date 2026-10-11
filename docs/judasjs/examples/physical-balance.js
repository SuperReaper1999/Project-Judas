import {input} from 'judas';

// Setup: a ready skinned entity with an explicitly authored two-chain balance
// mapping, ordinary ragdoll bodies/joints and optional CharacterMotor. Balance
// configuration alone does not activate physics. The Balance Lab uses the
// stand/recover/cancel logical actions. Tune the mapping for your actual rig.
export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}) { this.entity=entity; this.state={status:'idle',supported:false}; }
  fixedUpdate() {
    const r=this.entity.ragdoll;
    if(!r || !this.entity.animation?.info.ready)return;
    if(input.pressed('cancel')) {
      r.cancelBalance();
      // Explicit passive handoff preserves the current physical pose/motion.
      if(!r.active)r.setMode('passive',{motorHandoff:true});
    }
    if(input.pressed('stand'))r.requestBalance('stand',{stepping:true,effort:true,motorHandoff:true});
    if(input.pressed('recover'))r.requestBalance('recover',{stepping:true,effort:true,motorHandoff:true});
    const actual=r.balanceState;
    this.state.status=actual.status;
    this.state.supported=actual.available&&actual.supports.every(s=>s.contacts>0);
    // An accepted request is not completion. Only native measured state can
    // report succeeded. Do not replace this with a finished animation clock.
  }
}
