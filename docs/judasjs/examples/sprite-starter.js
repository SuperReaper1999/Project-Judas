// Native world sprites and an orthographic camera. Stage 1 movement is free
// XY movement: it deliberately does not pretend to provide 2D physics yet.
import {input, world} from 'judas';
export const properties = {
  speed: {type:'number',default:3},
  sideView: {type:'boolean',default:false},
  pixelPerfect: {type:'boolean',default:true}
};
export default class {
  /** @param {import('judas').ScriptContext<{speed:number;sideView:boolean;pixelPerfect:boolean}>} context */
  constructor({entity,properties}) {this.entity=entity;this.properties=properties;this.state={distance:0};}
  /** @param {number} dt */
  update(dt) {
    const x=input.axis('move_x'), y=input.axis('move_y');
    const length=Math.max(1,Math.hypot(x,y));
    const pose=this.entity.transform;
    pose.position.x+=x/length*this.properties.speed*dt;
    pose.position.y+=y/length*this.properties.speed*dt;
    this.entity.transform=pose;
    if(x!==0)this.entity.setSprite({flipX:x<0});
    this.state.distance+=Math.hypot(x,y)/length*this.properties.speed*dt;
  }
  presentationUpdate() {
    const p=this.entity.presentedTransform.position;
    world.setView({position:{x:p.x,y:p.y,z:10},rotation:{x:0,y:0,z:0,w:1}},60,
      {projection:'orthographic',height:8,near:.01,far:100,pixelPerfect:this.properties.pixelPerfect,
       referenceWidth:320,referenceHeight:180,pixelsPerUnit:32});
  }
  destroy(){world.clearView();}
}
