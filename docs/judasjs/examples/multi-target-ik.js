// Setup: the original CC0 multipart figure from projects/character_lab, with
// its ordinary Animation component. These names are fixture data: substitute
// your imported keys explicitly. No ragdoll or CharacterMotor is required.
export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}){this.entity=entity;this.state={configured:false,changed:false,cleared:false,iterations:0,targets:/** @type {import('judas').FullBodyIKResidual[]} */ ([]) };this.steps=0;}
  fixedUpdate(){
    const a=this.entity.animation;if(!a?.info.ready)return;
    if(!this.state.configured){
      this.state.configured=a.configureIK({
        bodyRoot:'Pivot',spine:['Vertebra0','Vertebra1'],rootRotation:true,
        rootMin:[-.15,-.15,-.15],rootMax:[.15,.15,.15],iterations:64,
        chains:[{id:'palmA',joints:['ShoulderA','ElbowA','PalmA']},{id:'palmB',joints:['ShoulderB','ElbowB','PalmB']},{id:'soleA',joints:['ThighA','ShinA','SoleA']},{id:'soleB',joints:['ThighB','ShinB','SoleB']}],
        limits:[{joint:'Vertebra0',frame:[0,0,0,1],min:[-.7,-.7,-.7],max:[.7,.7,.7],preferred:[0,0,0],preferenceWeight:0}],
        targets:[{id:'left-palm',chain:'palmA',space:'model',position:[-.80,.96,-.08],orientation:[0,0,0,1],positionWeight:1,orientationWeight:1,offset:[0,0,-.055]},
                 {id:'right-palm',chain:'palmB',space:'model',position:[.80,.96,-.08],orientation:[0,0,0,1],positionWeight:1,orientationWeight:1,offset:[0,0,-.055]},
                 {id:'left-sole',chain:'soleA',space:'model',position:[-.15,.03,-.1],orientation:[0,0,0,1],positionWeight:1,orientationWeight:1,offset:[0,-.04,-.1]},
                 {id:'right-sole',chain:'soleB',space:'model',position:[.15,.03,-.1],orientation:[0,0,0,1],positionWeight:1,orientationWeight:1,offset:[0,-.04,-.1]}]
      });
    }
    // Read the last completed solve. A configuration write cannot see future
    // physics/skeleton results. Never feed solved joints back into new targets.
    const status=a.ikStatus;this.state.iterations=status.iterations;this.state.targets=status.targets;
    if(++this.steps===12)this.state.changed=a.ikTargets([{id:'one-contact',chain:'palmA',space:'model',position:[-.78,.97,-.08],orientation:[0,0,0,1],positionWeight:1,orientationWeight:0,offset:[0,0,-.055]}]);
    if(this.steps===20)this.state.cleared=a.configureIK(null);
  }
}
