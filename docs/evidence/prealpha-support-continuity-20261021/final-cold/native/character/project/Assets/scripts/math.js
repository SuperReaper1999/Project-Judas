// Small project maths, never an IK or physics solver.
export const vec=(x=0,y=0,z=0)=>({x,y,z});
export const add=(a,b)=>vec(a.x+b.x,a.y+b.y,a.z+b.z);
export const sub=(a,b)=>vec(a.x-b.x,a.y-b.y,a.z-b.z);
export const scale=(a,s)=>vec(a.x*s,a.y*s,a.z*s);
export const arr=v=>[v.x,v.y,v.z];
export const quat=(x=0,y=0,z=0,w=1)=>({x,y,z,w});
export const qa=q=>[q.x,q.y,q.z,q.w];
export const axis=(v,t)=>quat(v.x*Math.sin(t/2),v.y*Math.sin(t/2),v.z*Math.sin(t/2),Math.cos(t/2));
export const multiply=(a,b)=>quat(a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z);
export const rotate=(q,v)=>{const u=quat(v.x,v.y,v.z,0),r=multiply(multiply(q,u),quat(-q.x,-q.y,-q.z,q.w));return vec(r.x,r.y,r.z);};
export const transformPoint=(t,p)=>add(t.position,rotate(t.rotation,vec(p.x*t.scale.x,p.y*t.scale.y,p.z*t.scale.z)));
export const mixPose=(a,b,t)=>{
  const dot=a.rotation.x*b.rotation.x+a.rotation.y*b.rotation.y+a.rotation.z*b.rotation.z+a.rotation.w*b.rotation.w;
  const sign=dot<0?-1:1,q=quat(a.rotation.x*(1-t)+sign*b.rotation.x*t,a.rotation.y*(1-t)+sign*b.rotation.y*t,a.rotation.z*(1-t)+sign*b.rotation.z*t,a.rotation.w*(1-t)+sign*b.rotation.w*t);
  const n=Math.hypot(q.x,q.y,q.z,q.w);q.x/=n;q.y/=n;q.z/=n;q.w/=n;
  return {position:add(scale(a.position,1-t),scale(b.position,t)),rotation:q};
};
