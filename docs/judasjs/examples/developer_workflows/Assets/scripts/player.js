import { world, input, physics, ui, scenes, saves } from 'judas';
import { crates, vehicles, players } from './registry.js';
const add = (a, b) => ({ x: a.x + b.x, y: a.y + b.y, z: a.z + b.z });
const mul = (a, s) => ({ x: a.x * s, y: a.y * s, z: a.z * s });
const rotate = (yaw, v) => ({
    x: Math.cos(yaw) * v.x + Math.sin(yaw) * v.z,
    y: v.y,
    z: -Math.sin(yaw) * v.x + Math.cos(yaw) * v.z,
});
const multiply = (a, b) => ({
    w: a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    x: a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
    y: a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
    z: a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
});
const axis = (key, angle) => ({
    w: Math.cos(angle / 2),
    x: 0,
    y: 0,
    z: 0,
    [key]: Math.sin(angle / 2),
});
// Quaternion rotation keeps the third-person boom in the same frame as its view.
const turn = (q, v) => {
    const r = multiply(multiply(q, { w: 0, ...v }), {
        w: q.w,
        x: -q.x,
        y: -q.y,
        z: -q.z,
    });
    return { x: r.x, y: r.y, z: r.z };
};
export default class Player {
    constructor({ entity }) {
        this.entity = entity;
        this.state = {
            yaw: 0,
            pitch: 0,
            third: false,
            driving: false,
            yardWanted: false,
        };
    }
    start() {
        this.bind();
    }
    restore() {
        this.bind();
    }
    bind() {
        players.set(this.entity.id, this);
        this.hud = ui.get('lesson');
        ui.debugOverlayVisible = false;
        this.hud.modal = false;
        input.pointerCapture = true;
        this.request = null;
        this.notice = 'Push the orange crate; load the blue yard with R.';
        this.demand = this.state.yardWanted
            ? scenes.requestRegion('yard')
            : null;
        saves.refresh();
    }
    uiUpdate() {
        if (input.pressed('pause')) {
            this.hud.modal = !this.hud.modal;
            input.pointerCapture = !this.hud.modal;
        }
        if (input.pressed('region')) {
            this.state.yardWanted = !this.state.yardWanted;
            if (this.state.yardWanted)
                this.demand = scenes.requestRegion('yard');
            else {
                if (this.demand) scenes.releaseRegion(this.demand);
                this.demand = null;
                scenes.unloadRegion('yard');
            }
        }
        if (input.pressed('save') || input.pressed('load')) {
            try {
                this.request = input.pressed('save')
                    ? saves.save('lesson')
                    : saves.load('lesson');
            } catch (error) {
                this.notice = error.message;
            }
        }
        if (this.request) {
            const status = saves.status(this.request);
            this.notice =
                status?.error || `Save/load: ${status?.state || 'queued'}`;
        }
        const region = scenes.regions.find((r) => r.id === 'yard');
        const entity = scenes.resolveRegionEntity('yard', '10');
        // 'active' describes native publication, not the script instance's binding.
        const registered = entity?.valid && crates.has(entity.id);
        const root = crates.get('200');
        this.hud.get('lesson').text =
            `DEVELOPER WORKFLOWS\nWASD move/drive · mouse look · Space jump/brake · V view · E push crate / board / exit\nR request/release yard (also while paused) · Escape pause · F6 save · F9 load\n` +
            `Yard: ${region?.error || region?.state} / visual ${region?.visualReady ? 'ready' : 'pending'} / script ${registered ? 'registered' : 'not registered'}\n` +
            `Root crate pushes ${root?.state.pushes || 0} · Yard pushes ${registered ? crates.get(entity.id).state.pushes : 'unavailable'}\n` +
            `${this.hud.modal ? 'PAUSED: preparation continues; native installation waits for resume.\n' : ''}${this.notice}`;
    }
    update(dt) {
        if (this.hud.modal) return;
        this.state.yaw -= input.axis('look_x') * 0.0021;
        this.state.pitch = Math.max(
            -1.3,
            Math.min(1.3, this.state.pitch - input.axis('look_y') * 0.0021),
        );
        if (input.pressed('camera_toggle'))
            this.state.third = !this.state.third;
    }
    fixedUpdate(dt) {
        if (this.hud.modal) return;
        const car = vehicles.get('7000');
        if (input.pressed('interact')) {
            if (this.state.driving) {
                if (
                    car &&
                    Math.hypot(
                        car.entity.velocity.x,
                        car.entity.velocity.y,
                        car.entity.velocity.z,
                    ) < 2
                ) {
                    // Boarding/exiting is explicit placement, never the driving mechanism.
                    this.entity.transform = {
                        position: add(car.entity.transform.position, {
                            x: 3,
                            y: 0,
                            z: 0,
                        }),
                    };
                    this.entity.character.enabled = true;
                    this.state.driving = false;
                } else this.notice = 'Stop before exiting.';
            } else {
                const delta =
                    car &&
                    add(
                        car.entity.transform.position,
                        mul(this.entity.transform.position, -1),
                    );
                if (
                    delta &&
                    Math.hypot(delta.x, delta.y, delta.z) < 3 &&
                    car.entity.powertrain?.state.available
                ) {
                    this.entity.character.enabled = false;
                    this.state.driving = true;
                } else {
                    const eye = add(this.entity.transform.position, {
                        x: 0,
                        y: 1.6,
                        z: 0,
                    });
                    const direction = rotate(this.state.yaw, {
                        x: 0,
                        y: Math.sin(this.state.pitch),
                        z: -Math.cos(this.state.pitch),
                    });
                    const hit = physics.raycast(eye, direction, 8, {
                        ignored: [this.entity],
                    });
                    const target = hit?.entity && crates.get(hit.entity.id);
                    if (target) target.push(direction);
                }
            }
        }
        if (this.state.driving) return;
        const motor = this.entity.character;
        const direction = rotate(this.state.yaw, {
            x: input.axis('move_x'),
            y: 0,
            z: -input.axis('move_y'),
        });
        const n = Math.max(1, Math.hypot(direction.x, direction.z));
        motor.velocity = {
            x: (direction.x / n) * 5,
            y: motor.velocity.y,
            z: (direction.z / n) * 5,
        };
        if (motor.supported && input.pressed('jump'))
            motor.velocity = { ...motor.velocity, y: 5 };
    }
    destroy() {
        if (players.get(this.entity.id) === this)
            players.delete(this.entity.id);
    }
    presentationUpdate() {
        const car = this.state.driving && vehicles.get('7000');
        const t = car
            ? car.entity.presentedTransform
            : this.entity.presentedTransform;
        const rotation = multiply(
            multiply(t.rotation, axis('y', this.state.yaw)),
            axis('x', this.state.pitch),
        );
        // Deliberately small flat-scene example. See camera.js in Lunar Island for
        // gravity-relative offsets and collision-aware third-person boom queries.
        const offset = this.state.third
            ? turn(rotation, { x: 0, y: 3, z: 8 })
            : { x: 0, y: car ? 1.2 : 1.6, z: 0 };
        world.setView({ position: add(t.position, offset), rotation }, 74, {
            near: 0.07,
            far: 250,
        });
    }
}
