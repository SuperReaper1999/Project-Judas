import {saves} from 'judas';
// Installed policy decides which exact older content may migrate. This script
// is game policy: invoke beginUpgrade from an explicit UI/input action.
export default class {
  constructor() { this.state={upgradeReady:false,requested:false,error:''};this.request=0; }
  /** @param {import('judas').UIEvent} event */
  onUI(event) { if(event.type==='click' && event.element==='upgrade')this.beginUpgrade(); }
  beginUpgrade() { this.request=saves.load('before-update',{migrate:true});this.state.requested=true; }
  update() {
    if(!this.request)return;
    const result=saves.status(this.request);
    if(result?.state==='failed'){this.state.error=result.error;this.request=0;}
    if(result?.state==='completed'){
      this.state.upgradeReady=result.migrated;this.request=0;
      // Save to a separate destination from an explicit game action.
    }
  }
}
