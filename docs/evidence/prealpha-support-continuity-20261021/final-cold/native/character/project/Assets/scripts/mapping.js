// These joint names are authored fixture data, never native anatomical rules.
export function mapping(names) {
  const chains=[['palmA','ShoulderA','ElbowA','PalmA'],['palmB','ShoulderB','ElbowB','PalmB'],['soleA','ThighA','ShinA','SoleA'],['soleB','ThighB','ShinB','SoleB']];
  const joints=['Pivot','Vertebra0','Vertebra1','ShoulderA','ElbowA','PalmA','ShoulderB','ElbowB','PalmB','ThighA','ShinA','SoleA','ThighB','ShinB','SoleB'];
  return {
    enabled:true,bodyRoot:names.Pivot,rootRotation:true,rootMin:[-.18,-.22,-.18],rootMax:[.18,.22,.18],
    spine:[names.Vertebra0,names.Vertebra1],iterations:64,damping:.02,positionTolerance:.005,orientationTolerance:.017453293,
    chains:chains.map(([id,...keys])=>({id,joints:keys.map(k=>names[k])})),
    limits:joints.map(k=>({joint:names[k],frame:[0,0,0,1],min:[-2.6,-2.6,-2.6],max:[2.6,2.6,2.6],preferred:[0,0,0],preferenceWeight:0})),
    targets:[]
  };
}
export function physicalSettings(names,effort=1){
  return {enabled:true,regions:[{id:'upper',joints:['Vertebra0','Vertebra1','ShoulderA','ElbowA','PalmA','ShoulderB','ElbowB','PalmB'].map(k=>names[k]),enabled:true,stiffness:32,damping:5,maxTorque:24,effortWeight:effort,poseWeight:1}]};
}
export const contact=(id,chain,position,orientation=[0,0,0,1],offset=[0,-.04,-.1])=>({id,chain,space:'world',enabled:true,position,orientation,positionWeight:1,orientationWeight:1,offset,frame:[0,0,0,1]});
