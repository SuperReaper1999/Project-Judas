// Attach to an authored Wheel; define drive/steer axes and brake action in the project.
import {input} from 'judas';
export default class {
  /** @param {{entity: import('judas').Entity}} context */
  constructor({entity}) { this.entity = entity; this.state = {contact: false, spin: 0}; }
  fixedUpdate() {
    const wheel = this.entity.wheel;
    if (!wheel) return;
    wheel.set({driveTorque: 140 * input.axis('drive'),
      brakeTorque: input.held('brake') ? 240 : 0,
      steeringAngle: .4 * input.axis('steer')});
    const physical = wheel.state;
    this.state.contact = physical.contact;
    this.state.spin = physical.angularVelocity;
  }
  destroy() { /* World ownership retires the wheel; no native body deletion here. */ }
}
