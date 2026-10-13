// Attach to a Deformable whose authored 'patch' group has a finite-strength
// dynamic Body attachment. The body's physical load determines breakage.
// Bind the project-owned logical action 'test_load' to a suitable control.
import {input} from 'judas';

export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}) {
    this.entity = entity;
    this.state = {loaded:false, released:false, peakForce:0, peakTorque:0};
  }

  fixedUpdate() {
    const material = this.entity.deformable;
    if (!material || !material.valid) return;
    const patch = material.state.attachments.find(a => a.group === 'patch');
    if (patch) {
      this.state.released = patch.released;
      this.state.peakForce = patch.peakForce;   // N, actual completed-step demand.
      this.state.peakTorque = patch.peakTorque; // N m, about its material patch.
    }
    if (!this.state.loaded && input.held('test_load')) {
      // A total physical impulse, not a scripted release or visual animation.
      material.impulse({x:0.2, y:0, z:0}, 'patch');
      this.state.loaded = true;
    }
  }
}
