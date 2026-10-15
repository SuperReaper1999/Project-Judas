import {signals} from 'judas';
export default class Capsule {
 constructor({entity}){this.entity=entity;this.state={used:false};}
 onCollisionEnter(event){if(this.state.used||!event.other?.valid||!event.other.hasTag('paddle'))return;
  this.state.used=true;signals.emit('breakwater.power');this.entity.destroy();
 }
 fixedUpdate(){if(this.entity.transform.position.y < -13)this.entity.destroy();}
}
