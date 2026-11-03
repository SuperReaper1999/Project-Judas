import {console, navigation} from 'judas';

const destination = {x: 8, y: 0, z: 0};

export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}) {
    this.entity = entity;
    this.started = false;
    this.arrived = false;
  }

  fixedUpdate() {
    const agent = this.entity.navigation;
    const motor = this.entity.character;
    if (!agent || !motor || this.arrived) return;

    if (!this.started) {
      // The baked asset loads asynchronously. Wait for a complete route.
      const start = navigation.sample({x: -8, y: 0, z: 0});
      if (!start || navigation.path(start.position, destination).status !== 'complete') return;
      if (!agent.setDestination(destination)) return;
      this.started = true;
    }

    const {up, velocity} = motor;
    const vertical = velocity.x * up.x + velocity.y * up.y + velocity.z * up.z;
    const arrived = agent.state.reached;
    const steer = arrived ? {x: 0, y: 0, z: 0} : agent.steering;
    motor.velocity = {
      x: steer.x + up.x * vertical,
      y: steer.y + up.y * vertical,
      z: steer.z + up.z * vertical
    };

    const {x, y, z} = this.entity.transform.position;
    if (arrived && Math.hypot(x - destination.x, z - destination.z) <= 0.5 && y > 0.6 && y < 1.2) {
      this.arrived = true;
      console.log('TERRAIN_NAV_COURIER_ARRIVED');
    }
  }
}
