export const add=(a,b)=>({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
export const mul=(a,s)=>({x:a.x*s,y:a.y*s,z:a.z*s});
export const sub=(a,b)=>add(a,mul(b,-1));
export const dot=(a,b)=>a.x*b.x+a.y*b.y+a.z*b.z;
export const length=a=>Math.sqrt(dot(a,a));
export const norm=a=>length(a)>1e-7?mul(a,1/length(a)):{x:0,y:0,z:0};
export const bounded=(a,n)=>length(a)>n?mul(a,n/length(a)):a;
export const qm=(a,b)=>({w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w});
export const inverse=q=>({w:q.w,x:-q.x,y:-q.y,z:-q.z});
export const rotate=(q,v)=>{const a=qm(qm(q,{w:0,...v}),inverse(q));return {x:a.x,y:a.y,z:a.z}};
export const axis=(v,r)=>({w:Math.cos(r/2),...mul(v,Math.sin(r/2))});
export const matrix=(m,v)=>add(add(mul(m.x,v.x),mul(m.y,v.y)),mul(m.z,v.z));
export function attitude(target,current){let q=qm(target,inverse(current));if(q.w<0)q={w:-q.w,x:-q.x,y:-q.y,z:-q.z};const n=Math.hypot(q.x,q.y,q.z);return n>1e-6?mul(q,2*Math.atan2(n,q.w)/n):{x:0,y:0,z:0};}
