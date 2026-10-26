import { input } from 'judas';
import { vehicles, players } from './registry.js';
export default class Vehicle {
    constructor({ entity }) {
        this.entity = entity;
        this.state = { secondsDriven: 0 };
    }
    start() {
        vehicles.set(this.entity.id, this);
    }
    restore() {
        vehicles.set(this.entity.id, this);
    }
    fixedUpdate(dt) {
        const pilot = players.get('100')?.state.driving;
        const throttle = pilot ? input.axis('move_y') : 0;
        const steer = pilot ? input.axis('move_x') : 0;
        const power = this.entity.powertrain;
        const braking = !pilot || input.held('jump');
        if (!power?.state.available) return;
        power.set({
            sources: [
                {
                    name: 'engine',
                    input: Math.abs(throttle),
                    starter:
                        pilot && Math.abs(power.state.shafts[0].rpm) < 850
                            ? 1
                            : 0,
                },
            ],
            links: [
                { name: 'gearbox', ratio: (throttle < 0 ? -1 : 1) / 4.5 },
                { name: 'clutch', engagement: pilot ? 1 : 0 },
            ],
            brakes: [
                { name: 'left', engagement: braking ? 1 : 0 },
                { name: 'right', engagement: braking ? 1 : 0 },
            ],
        });
        this.entity.children
            .filter((e) => e.wheel)
            .forEach((wheel, i) => {
                wheel.wheel.set({ steeringAngle: i < 2 ? -steer * 0.38 : 0 });
            });
        if (pilot) this.state.secondsDriven += dt;
        // Do not copy presentedTransform to a visual child. Its hierarchy already
        // follows native interpolated chassis/wheel poses, including while paused.
    }
    destroy() {
        if (vehicles.get(this.entity.id) === this)
            vehicles.delete(this.entity.id);
    }
}
