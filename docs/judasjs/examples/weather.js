// Attach to an entity with an authored Weather region component.
import {weather} from 'judas';

export default class {
  /** @param {{entity:import('judas').Entity}} context */
  constructor({entity}) {
    this.entity=entity;
    this.state={started:false,windSpeed:0,steps:0};
  }
  fixedUpdate() {
    const region=this.entity.weather;
    if(!this.state.started&&region?.valid) {
      // JS wind is a world-space vector. This example chooses its own direction.
      region.transition({rain:.8,cloudCoverage:.95,wind:{x:8,y:0,z:2},gust:5},12);
      this.state.started=true;
    }
    const air=weather.sample(this.entity.transform.position);
    if(air)this.state.windSpeed=Math.hypot(air.wind.x,air.wind.y,air.wind.z);
    ++this.state.steps;
  }
}
