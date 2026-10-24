import {console} from 'judas';
export default class {
 constructor({entity}) { this.entity=entity; }
 start(){this.entity.velocity={x:100,y:0,z:0};this.entity.angularVelocity={x:0,y:0,z:100};}
 presentationUpdate(){console.log('ENERGY_ENDPOINT '+JSON.stringify({v:this.entity.velocity,w:this.entity.angularVelocity,q:this.entity.transform.rotation}));}
}
