import {input} from 'judas';
export default class {
  /** @param {{entity:import('judas').Entity}} context */
  constructor({entity}) { this.entity=entity; }
  fixedUpdate() {
    // Attach to a graph authoring the named engine and axleBrake members.
    // Driver policy is project JS; the engine evolves actual shaft motion.
    this.entity.powertrain?.set({
      sources:[{name:'engine',input:Math.max(0,input.axis('drive'))}],
      brakes:[{name:'axleBrake',engagement:input.held('brake')?1:0}]
    });
  }
}
