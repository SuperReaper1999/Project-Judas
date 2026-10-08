import {world, physics} from 'judas';
export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}) { this.entity=entity;this.state={pose:false,ik:false,socket:false,unsocket:false,gravity:false,material:false,joint:false,stale:false}; }
  fixedUpdate() {
    const animation=this.entity.animation;
    if (!animation?.info.ready || this.state.pose) return;
    const foot=animation.jointTransform('LeftFoot','world');
    if (!foot) return;
    this.state.pose=true;
    this.state.ik=animation.limb('example',{root:'LeftHip',middle:'LeftKnee',end:'LeftFoot',target:foot.position,pole:{x:5,y:1,z:3},weight:1,order:2});
    const prop=world.entity('102');
    this.state.socket=!!prop?.setSocket(this.entity,'Hand',{position:{x:.15,y:0,z:0}});
    // null removes the socket (documented); then re-attach so the rest of the fixture is unchanged.
    this.state.unsocket=prop?.setSocket(null)===true&&!!prop.setSocket(this.entity,'Hand',{position:{x:.15,y:0,z:0}});
    const body=world.entity('400'),owner=world.entity('401');
    if (!body || !owner) return;
    const g=physics.gravity(body.transform.position);
    this.state.gravity=Math.abs(g.y+9.81)<.001;
    body.setPhysicalMaterial(null,{friction:.7,restitution:.1});
    this.state.material=Math.abs((body.physicalMaterial?.friction??0)-.7)<.001;
    const old=physics.joint(owner);if (old) old.destroy();
    const joint=physics.createJoint(owner,{type:'ball',bodyA:body,anchorB:{x:0,y:0,z:0}});
    this.state.joint=joint.configure({anchorB:{x:0,y:1,z:0}});
    joint.destroy();this.state.stale=!joint.valid;
    animation.removeLimb('example');
  }
}
