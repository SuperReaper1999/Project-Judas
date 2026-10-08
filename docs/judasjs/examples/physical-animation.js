// Setup: ordinary CharacterMotor + original multipart figure + authored
// ragdoll body/constraint mapping from projects/character_lab. Physics bodies
// remain the actual result; this script only requests modes and an impact.
export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}){this.entity=entity;this.steps=0;this.state={configured:false,partial:false,impulse:false,active:false,passive:false,returned:false,mode:'animation'};}
  fixedUpdate(){
    const e=this.entity,a=e.animation,r=e.ragdoll;if(!a?.info.ready||!r)return;
    if(!this.state.configured){
      this.state.configured=r.configurePhysical({enabled:true,regions:[{id:'arm',joints:['ShoulderA','ElbowA','PalmA'],enabled:true,stiffness:30,damping:4,maxTorque:20,effortWeight:1,poseWeight:1}]});
      this.state.partial=r.setMode('partial',{fade:.2});
    }
    this.state.mode=r.physicalState.mode;
    if(++this.steps===15){const body=r.body('ElbowA');if(body?.valid){body.applyImpulseAtPoint({x:2,y:.2,z:-1},body.transform.position);this.state.impulse=true;}}
    if(this.steps===35)this.state.active=r.setMode('active',{fade:.2,motorHandoff:true});
    // Removes only active drive; keeps current physical pose/body velocities.
    if(this.steps===55)this.state.passive=r.setMode('passive',{fade:.2,motorHandoff:true});
    if(this.steps===80){const p=e.transform.position;this.state.returned=r.setMode('animation',{fade:.4,resumeMotor:true,placement:{position:[p.x,1.1,p.z],rotation:[0,0,0,1]}});}
    // A queued request can be refused at the fixed boundary. Inspect the copied
    // state/diagnostic rather than treating the request's true as completion.
    this.state.mode=r.physicalState.mode;
  }
}
