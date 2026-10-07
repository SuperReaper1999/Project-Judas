import {input} from 'judas';

/** Author a vector named flick_stick using stick:Right, circular=true, deadzone=.2.
 * Frame values are for presentation. Ordered observations are processed once at
 * fixed boundaries; recognizing a trick from them remains project behaviour. */
export default class {
  constructor() {
    this.cursor=0;
    this.state={raw:{x:0,y:0},circular:{x:0,y:0},frameDelta:{x:0,y:0},
      samples:/** @type {import('judas').StickSample[]} */([]),observations:0,
      resets:0,overflows:0,capacity:128,sequence:0,frameReads:0,fixedReads:0};
  }
  update() {
    this.state.raw=input.stick('right');
    this.state.circular=input.vector('flick_stick');
    this.state.frameDelta=input.stickDelta('right');
    ++this.state.frameReads;
  }
  fixedUpdate() {
    const snapshot=input.stickSamples('right',this.cursor);
    if(snapshot.reset||snapshot.overflow) {
      // Do not connect old recognizer motion across a lifecycle reset or gap.
      this.state.samples=[];
      this.state.resets+=snapshot.reset?1:0;
      this.state.overflows+=snapshot.overflow?1:0;
    }
    // Each reader keeps its own cursor; this never consumes another reader's data.
    for(const observation of snapshot.samples) {
      this.state.samples.push(observation);
      ++this.state.observations;
    }
    this.state.samples=this.state.samples.slice(-12); // Small display only.
    this.cursor=snapshot.sequence;
    this.state.sequence=snapshot.sequence;
    this.state.capacity=snapshot.capacity;
    ++this.state.fixedReads;
  }
}
