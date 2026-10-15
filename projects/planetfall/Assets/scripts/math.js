// Ordinary project vector helpers; no engine internals or implicit world-up.
export const add=(a,b)=>({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
export const mul=(a,s)=>({x:a.x*s,y:a.y*s,z:a.z*s});
export const sub=(a,b)=>add(a,mul(b,-1));
export const dot=(a,b)=>a.x*b.x+a.y*b.y+a.z*b.z;
export const length=a=>Math.sqrt(dot(a,a));
export const norm=a=>length(a)>1e-6?mul(a,1/length(a)):{x:0,y:0,z:0};
export const tangent=(a,u)=>sub(a,mul(u,dot(a,u)));
export const qm=(a,b)=>({w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w});
export const axis=(a,t)=>({w:Math.cos(t/2),...mul(a,Math.sin(t/2))});
export const rotate=(q,v)=>{const r=qm(qm(q,{w:0,...v}),{w:q.w,x:-q.x,y:-q.y,z:-q.z});return {x:r.x,y:r.y,z:r.z};};
