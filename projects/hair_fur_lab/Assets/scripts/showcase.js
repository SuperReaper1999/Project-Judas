import { input, world } from 'judas';

const vec = (x, y, z) => ({x, y, z});
const rotation = (pitch, yaw) => ({
    w: Math.cos(yaw/2)*Math.cos(pitch/2), x: Math.cos(yaw/2)*Math.sin(pitch/2),
    y: Math.sin(yaw/2)*Math.cos(pitch/2), z: -Math.sin(yaw/2)*Math.sin(pitch/2)
});

export default class {
    constructor() { this.state = {elapsed: 0, snap: 0}; }
    fixedUpdate(dt) {
        this.state.elapsed += dt;
        if (input.pressed('snap')) this.state.snap=1.25;
        this.state.snap *= Math.exp(-8*dt);
        const t = this.state.elapsed;
        const human = world.entity('30');
        if (human?.valid) human.transform = {
            position: vec(2.1 + .24*Math.sin(t*3), 0, 0),
            rotation: rotation(0, .55*Math.sin(t*3.5)+this.state.snap)
        };
    }
    presentationUpdate() {
        const t = this.state.elapsed, cycle = t % 18;
        const close = input.held('close'), far = input.held('far');
        const z = close ? 2.55 : far ? 22 : 5.9 + 1.5*Math.sin(t*.24);
        const x = close ? (cycle < 6 ? -2.55 : cycle < 12 ? -.25 : 2.1) : .25*Math.sin(t*.45);
        const height = close ? (cycle < 6 ? 1.21 : cycle < 12 ? 1.6 : 1.85) : far ? 4.5 : 2.28;
        world.setView({position: vec(x, height, z),
                       rotation: rotation(close ? -.09 : far ? -.15 : -.19, 0)}, 52, {near: .075, far: 80});
    }
}
