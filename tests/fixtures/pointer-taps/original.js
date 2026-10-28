// Extracted BlockSmasherV2 pointer detector; isolated from unrelated game systems.
import {input, world} from 'judas';
const TAP_MOVE=.03, TAP_TIME=.3;
export default class {
 constructor(){this.state={time:0,phase:'aim',taps:0,drags:0};this.presses=new Map();this.touchMode=false;this.waitRelease=false;}
 fixedUpdate(dt){this.state.time+=dt;this.state.taps+=this.pointerInput();}
 screenToBoard(x,y){return {x,y};}
 aimAt(){this.state.drags++;}
    pointerInput() {
        const s = this.state, live = input.touches.map((t) => ({ id: 't' + t.id, x: t.x, y: t.y }));
        if (live.length) this.touchMode = true;
        if (!this.touchMode && input.pointer.down) live.push({ id: 'mouse', x: input.pointer.x, y: input.pointer.y });
        // The finger/click that closed a menu must lift before it counts.
        if (this.waitRelease && !live.length) this.waitRelease = false;
        const vp = world.viewport, short = Math.max(1, Math.min(vp.width, vp.height)), seen = new Set();
        let taps = 0;
        for (const p of live) {
            seen.add(p.id);
            let press = this.presses.get(p.id);
            if (!press) {
                // Presses on the HUD bars, during a shot or left over from a menu are ignored.
                const ignore = this.waitRelease || s.phase !== 'aim' || p.y < 0.11 || p.y > 0.92;
                press = { x: p.x, y: p.y, t: s.time, drag: false, ignore };
                this.presses.set(p.id, press);
            }
            if (press.ignore) continue;
            const moved = Math.hypot((p.x - press.x) * vp.width, (p.y - press.y) * vp.height) / short;
            if (!press.drag && (moved > TAP_MOVE || s.time - press.t > TAP_TIME)) press.drag = true;
            if (press.drag && s.phase === 'aim') {
                const point = this.screenToBoard(p.x, p.y);
                if (point) this.aimAt(point);
            }
        }
        for (const [id, press] of this.presses) {
            if (seen.has(id)) continue;
            this.presses.delete(id);
            if (!press.ignore && !press.drag) taps++;
        }
        return taps;
    }

}
