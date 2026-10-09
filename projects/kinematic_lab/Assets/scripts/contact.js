// A normal script on the moving collider receives the existing filtered
// body-pair callbacks; it does not manufacture contact or move its neighbour.
export default class {
  constructor(){this.state={enter:0,stay:0,exit:0,lastOther:'',impulse:0};}
  onCollisionEnter(e){this.state.enter++;this.record(e);}
  onCollisionStay(e){this.state.stay++;this.record(e);}
  onCollisionExit(e){this.state.exit++;this.record(e);}
  record(e){this.state.lastOther=e.other?.id??'';this.state.impulse=e.normalImpulse??0;}
}
