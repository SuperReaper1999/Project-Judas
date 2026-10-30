import {input} from 'judas';

// Assign a .judassprite with idle/interact clips in the Sprite inspector first.
export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}) {
    this.entity=entity;
    this.state={strikes:0};
  }
  fixedUpdate() {
    const animation=this.entity.spriteAnimation;
    if(!animation)return;
    if(animation.error)throw new Error(animation.error);
    if(!animation.ready)return;
    // Previous completed fixed sample: dispatch once here, never per render.
    for(const event of animation.events)if(event.name==='strike')this.state.strikes++;
    if(input.pressed('interact')) {
      this.entity.setSpriteAnimation({clip:'interact',time:0,playing:true,loop:false});
    }else if(animation.finished) {
      this.entity.setSpriteAnimation({clip:'idle',time:0,playing:true,loop:true});
    }
  }
}
