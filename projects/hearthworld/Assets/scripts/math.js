// Ordinary game-side math. All physical directions are world-frame vectors.
export const add = (a, b) => ({ x: a.x + b.x, y: a.y + b.y, z: a.z + b.z });
export const mul = (a, s) => ({ x: a.x * s, y: a.y * s, z: a.z * s });
export const sub = (a, b) => add(a, mul(b, -1));
export const dot = (a, b) => a.x * b.x + a.y * b.y + a.z * b.z;
export const cross = (a, b) => ({ x: a.y * b.z - a.z * b.y, y: a.z * b.x - a.x * b.z, z: a.x * b.y - a.y * b.x });
export const length = a => Math.sqrt(dot(a, a));
export const norm = a => length(a) > 1e-8 ? mul(a, 1 / length(a)) : { x: 0, y: 0, z: 0 };
export const bounded = (a, n) => length(a) > n ? mul(a, n / length(a)) : a;
export const tangent = (a, u) => sub(a, mul(u, dot(a, u)));
export const vec = a => Array.isArray(a) ? { x: a[0], y: a[1], z: a[2] } : a;
export const inverse = q => ({ w: q.w, x: -q.x, y: -q.y, z: -q.z });
export const qm = (a, b) => ({ w: a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, x: a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, y: a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, z: a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w });
export const axis = (a, t) => ({ w: Math.cos(t / 2), ...mul(a, Math.sin(t / 2)) });
export const rotate = (q, v) => { const r = qm(qm(q, { w: 0, ...v }), inverse(q)); return { x: r.x, y: r.y, z: r.z }; };
export function fromTo(a, b) { const d = dot(a, b); if (d < -.99999)
    return axis(norm(cross(a, Math.abs(a.x) < .8 ? { x: 1, y: 0, z: 0 } : { x: 0, y: 0, z: 1 })), Math.PI); const c = cross(a, b), w = 1 + d, n = Math.sqrt(w * w + dot(c, c)); return { w: w / n, ...mul(c, 1 / n) }; }
