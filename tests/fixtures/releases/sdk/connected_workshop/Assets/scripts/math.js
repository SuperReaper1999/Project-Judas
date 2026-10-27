export const v=(x=0,y=0,z=0)=>({x,y,z});
export const add=(a,b)=>v(a.x+b.x,a.y+b.y,a.z+b.z);
export const sub=(a,b)=>v(a.x-b.x,a.y-b.y,a.z-b.z);
export const mul=(a,s)=>v(a.x*s,a.y*s,a.z*s);
export const len=a=>Math.hypot(a.x,a.y,a.z);
export const norm=a=>mul(a,1/Math.max(.001,len(a)));
const cross=(a,b)=>v(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);
// Ordinary project camera policy, independent of bodies and player identity.
export function camera(eye,target){
 const z=norm(sub(eye,target)),x=norm(cross(v(0,1,0),z)),y=cross(z,x),m=[x.x,y.x,z.x,x.y,y.y,z.y,x.z,y.z,z.z];
 let w,qx,qy,qz,s;const t=m[0]+m[4]+m[8];
 if(t>0){s=Math.sqrt(t+1)*2;w=s/4;qx=(m[7]-m[5])/s;qy=(m[2]-m[6])/s;qz=(m[3]-m[1])/s;}
 else if(m[0]>m[4]&&m[0]>m[8]){s=Math.sqrt(1+m[0]-m[4]-m[8])*2;w=(m[7]-m[5])/s;qx=s/4;qy=(m[1]+m[3])/s;qz=(m[2]+m[6])/s;}
 else if(m[4]>m[8]){s=Math.sqrt(1+m[4]-m[0]-m[8])*2;w=(m[2]-m[6])/s;qx=(m[1]+m[3])/s;qy=s/4;qz=(m[5]+m[7])/s;}
 else{s=Math.sqrt(1+m[8]-m[0]-m[4])*2;w=(m[3]-m[1])/s;qx=(m[2]+m[6])/s;qy=(m[5]+m[7])/s;qz=s/4;}
 return {position:eye,rotation:{w,x:qx,y:qy,z:qz}};
}
