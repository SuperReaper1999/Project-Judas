// Attach to a render-only box at (0, 2, 0); no Body or input map is required.
import {console, world} from 'judas';

export const properties = {
  rotationSpeed: {type: 'number', default: 1.0}
};

export default class {
  /** @param {import('judas').ScriptContext<{rotationSpeed: number}>} context */
  constructor({entity, properties}) {
    this.entity = entity;
    this.properties = properties;
    this.state = {elapsed: 0, frames: 0, presentations: 0};
  }

  start() { console.log('My first Judas project is running.'); }

  /** @param {number} dt */
  update(dt) {
    this.state.elapsed += dt * this.properties.rotationSpeed;
    this.state.frames++;
    const pose = this.entity.transform;
    // This example chooses a local Y spin; the engine has no universal up.
    pose.rotation = {x: 0, y: Math.sin(this.state.elapsed / 2),
                     z: 0, w: Math.cos(this.state.elapsed / 2)};
    this.entity.transform = pose;
  }

  presentationUpdate() {
    world.setView({position: {x: 0, y: 2, z: 6},
                   rotation: {x: 0, y: 0, z: 0, w: 1}}, 65);
    this.state.presentations++;
  }

  destroy() { world.clearView(); }
}
