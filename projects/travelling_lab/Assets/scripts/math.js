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

export const cross=(a,b)=>({x:a.y*b.z-a.z*b.y,y:a.z*b.x-a.x*b.z,z:a.x*b.y-a.y*b.x});
export const tangent=(a,n)=>sub(a,mul(n,dot(a,n)));
export function look(forward,up){const z=mul(norm(forward),-1),x=norm(cross(up,z)),y=cross(z,x);const m=[x.x,y.x,z.x,x.y,y.y,z.y,x.z,y.z,z.z],trace=m[0]+m[4]+m[8];let q;
 if(trace>0){const s=Math.sqrt(trace+1)*2;q={w:s/4,x:(m[7]-m[5])/s,y:(m[2]-m[6])/s,z:(m[3]-m[1])/s};}
 else {let i=m[4]>m[0]?1:0;if(m[8]>m[i*3+i])i=2;const j=(i+1)%3,k=(i+2)%3,s=Math.sqrt(1+m[i*3+i]-m[j*3+j]-m[k*3+k])*2;const v=[0,0,0];v[i]=s/4;v[j]=(m[i*3+j]+m[j*3+i])/s;v[k]=(m[i*3+k]+m[k*3+i])/s;q={w:(m[k*3+j]-m[j*3+k])/s,x:v[0],y:v[1],z:v[2]};}return q;
}
