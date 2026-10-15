import {signals,world,saves} from 'judas';
export default class Ball {
 constructor({entity}){this.entity=entity;this.state={launched:false};}
 start(){this.entity.gravity.setUniform({x:0,y:0,z:0});}
 restore(){this.start();}
 onCollisionEnter(event){
  if(!event.other?.valid)return;
  if(event.other.hasTag('brick'))signals.emit('breakwater.hit',{reference:saves.reference(event.other),point:event.point});
  if(event.other.hasTag('paddle')){
   // The solver supplies the bounce. Game policy adds an ordinary impulse to
   // steer it according to where the ball struck the curved-looking paddle.
   const offset=this.entity.transform.position.x-event.other.transform.position.x;
   const current=this.entity.velocity,speed=Math.min(19,Math.max(11,Math.hypot(current.x,current.y)));
   const x=Math.max(-.78,Math.min(.78,offset/2))*speed;
   this.entity.applyImpulse({x:(x-current.x)*this.entity.mass,y:(Math.sqrt(speed*speed-x*x)-current.y)*this.entity.mass,z:-current.z*this.entity.mass});
   signals.emit('breakwater.bounce');
  }
 }
 fixedUpdate(){
  // The level is a planar physical assembly: symmetric contacts and no Z forces.
  // No script relocates a travelling ball or replaces collision detection.
  const p=this.entity.transform.position;
  if(p.y < -12.8||Math.abs(p.x)>14||p.y>18){signals.emit('breakwater.lost');this.entity.destroy();}
 }
}
