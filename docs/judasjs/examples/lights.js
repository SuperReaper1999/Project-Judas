import {world} from 'judas';
export default class {
  constructor() { this.elapsed=0; }
  /** @param {number} dt */
  update(dt) {
    this.elapsed+=dt;
    const lamp=world.queryTags(['lamp'])[0];
    if(lamp?.light) lamp.setLight({enabled:true,intensity:1+0.2*Math.sin(this.elapsed)});
  }
}
