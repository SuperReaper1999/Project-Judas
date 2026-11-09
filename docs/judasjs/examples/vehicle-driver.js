// Ordinary project-owned controls. Copy into Assets/Scripts and attach to the
// chassis. Native wheels/powertrains own simulation; this never assigns motion.
import {input, world} from 'judas';

/** @param {import('judas').Vec3} a @param {import('judas').Vec3} b */
const add=(a,b)=>({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
/** @param {import('judas').Vec3} a @param {number} s */
const mul=(a,s)=>({x:a.x*s,y:a.y*s,z:a.z*s});
/** @param {import('judas').Vec3} a */
const length=a=>Math.hypot(a.x,a.y,a.z);
/** @param {import('judas').Vec3} a */
const norm=a=>mul(a,1/Math.max(1e-8,length(a)));
/** @param {import('judas').Vec3} a @param {import('judas').Vec3} b */
const cross=(a,b)=>({x:a.y*b.z-a.z*b.y,y:a.z*b.x-a.x*b.z,z:a.x*b.y-a.y*b.x});
/** @param {import('judas').Quat} q @param {import('judas').Vec3} v */
function rotate(q,v) {
    const t=mul(cross(q,v),2);
    return add(v,add(mul(t,q.w),cross(q,t)));
}
/** @param {import('judas').Vec3} forward @param {import('judas').Vec3} up */
function look(forward,up) {
    const z=mul(norm(forward),-1),x=norm(cross(up,z)),y=cross(z,x);
    const m=[x.x,y.x,z.x,x.y,y.y,z.y,x.z,y.z,z.z];
    const trace=m[0]+m[4]+m[8];
    if(trace>0){const s=Math.sqrt(trace+1)*2;return {w:s/4,x:(m[7]-m[5])/s,y:(m[2]-m[6])/s,z:(m[3]-m[1])/s};}
    let i=m[4]>m[0]?1:0;if(m[8]>m[i*3+i])i=2;
    const j=(i+1)%3,k=(i+2)%3,s=Math.sqrt(1+m[i*3+i]-m[j*3+j]-m[k*3+k])*2,v=[0,0,0];
    v[i]=s/4;v[j]=(m[i*3+j]+m[j*3+i])/s;v[k]=(m[i*3+k]+m[k*3+i])/s;
    return {w:(m[k*3+j]-m[j*3+k])/s,x:v[0],y:v[1],z:v[2]};
}

/** @param {import('judas').Entity} chassis */
export function wheelsOf(chassis) {
    return chassis.children.filter(child=>child.wheel);
}
/** @param {import('judas').Entity} chassis @param {number} throttle @param {number} steering */
export function driveVehicle(chassis,throttle,steering,brakeTorque=0,maximumSteer=.42) {
    const wheels=wheelsOf(chassis);
    const front=Math.min(...wheels.map(w=>w.wheel?.settings.mount.z??Infinity));
    const power=chassis.powertrain;
    if(power){const settings=power.settings,state=power.state;power.set({sources:settings.sources.map(source=>({
        name:source.name,
        input:source.kind==='combustion'?Math.abs(throttle):throttle,
        customTorque:source.kind==='custom'?throttle*source.maxTorque:0,
        starter:source.kind==='combustion'&&Math.abs(throttle)>.01&&Math.abs(state.shafts[source.shaft].rpm)<900?1:0
    })),links:settings.links.filter(link=>link.kind==='gear'&&link.name==='gearbox').map(link=>({
        name:link.name,ratio:Math.abs(link.ratio)*(throttle<0?-1:1)
    }))});}
    // Steering sensitivity is an editable input policy, not a hidden native
    // stability force. Replace this function for all-wheel/skid steering.
    const speed=length(chassis.velocity);
    const angle=steering*maximumSteer/(1+.003*speed*speed);
    for(const entity of wheels) {
        const wheel=entity.wheel;if(!wheel)continue;
        wheel.set({driveTorque:power?0:throttle*120,brakeTorque,
            steeringAngle:wheel.settings.mount.z<front+.05?angle:0});
    }
}
/** Project-owned two-pedal policy. Opposing the current travel applies service
 * brakes; release that pedal before selecting reverse. Native torque APIs stay
 * direct. State is serializable so a cold save retains a held braking pedal.
 * @param {{brakingDirection:number}} state @param {number} throttle
 * @param {number} speed Signed forward speed in m/s. @param {boolean} brakeHeld
 */
export function resolveVehiclePedals(state,throttle,speed,brakeHeld=false) {
    const direction=Math.abs(throttle)>.05?Math.sign(throttle):0;
    if(!direction)state.brakingDirection=0;
    if(direction&&direction*speed<-.2)state.brakingDirection=direction;
    const opposing=direction!==0&&state.brakingDirection===direction;
    return {drive:brakeHeld||opposing?0:throttle,brake:brakeHeld?1:opposing?Math.abs(throttle):0};
}
/** @param {import('judas').Entity} chassis */
export function vehicleForwardSpeed(chassis) {
    const forward=rotate(chassis.transform.rotation,{x:0,y:0,z:-1});
    const velocity=chassis.velocity;
    return forward.x*velocity.x+forward.y*velocity.y+forward.z*velocity.z;
}
/** @param {import('judas').Entity} chassis */
export function chaseVehicle(chassis) {
    const pose=chassis.presentedTransform;
    const gravity=chassis.gravity.acceleration;
    const up=length(gravity)>1e-5?mul(norm(gravity),-1):{x:0,y:1,z:0};
    const back=rotate(pose.rotation,{x:0,y:0,z:1});
    const eye=add(add(pose.position,mul(back,8)),mul(up,3.4));
    const target=add(pose.position,mul(up,.5));
    world.setView({position:eye,rotation:look(add(target,mul(eye,-1)),up)},65,{near:.1,far:700});
}

export const properties={
    driving:{type:'boolean',default:true},
    camera:{type:'boolean',default:true},
    maximumSteer:{type:'number',default:.42},
    brakeTorque:{type:'number',default:450}
};
export default class {
    /** @param {{entity:import('judas').Entity,properties:{driving:boolean,camera:boolean,maximumSteer:number,brakeTorque:number}}} context */
    constructor({entity,properties}){this.entity=entity;this.properties=properties;this.state={pedals:{brakingDirection:0}};}
    fixedUpdate(){
        const pedals=resolveVehiclePedals(this.state.pedals,this.properties.driving?input.axis('vehicle_throttle'):0,
            vehicleForwardSpeed(this.entity),input.held('vehicle_brake'));
        driveVehicle(this.entity,pedals.drive,
            this.properties.driving?-input.axis('vehicle_steer'):0,
            pedals.brake*this.properties.brakeTorque,this.properties.maximumSteer);
    }
    presentationUpdate(){if(this.properties.camera)chaseVehicle(this.entity);}
    destroy(){if(this.properties.camera)world.clearView();}
}
