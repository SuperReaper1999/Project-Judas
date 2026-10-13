// Focused application fixture only. Ordinary interactive scenes do not run it.
import {world} from 'judas';
export default class {
 constructor(){this.state={phase:0};this.power=null;}
 start(){
  this.power=world.entity('100').powertrain;
  this.state.valid=this.power.valid&&world.entity('101').powertrain===null;
  try{this.power.set({sources:[{name:'engine',input:1}]});this.state.startMutationBlocked=false;}
  catch(error){this.state.startMutationBlocked=error instanceof TypeError;}
 }
 fixedUpdate(){
  if(this.state.phase===0){
   const detached=this.power.settings;detached.shafts[0].inertia=999;
   this.state.detachedSettings=this.power.settings.shafts[0].inertia!==999;
   try{this.power.set({sources:[{name:'engine',input:2}]});this.state.invalidInputBlocked=false;}
   catch(error){this.state.invalidInputBlocked=error instanceof TypeError;}
   try{this.power.set({sources:[{name:'not-an-authored-source',input:.3}]});this.state.unknownSourceBlocked=false;}
   catch(error){this.state.unknownSourceBlocked=error instanceof TypeError;}
   try{this.power.set({links:[{name:'gearbox',ratio:0}]});this.state.invalidRatioBlocked=false;}
   catch(error){this.state.invalidRatioBlocked=error instanceof TypeError;}
   // @ts-expect-error Unknown keys must fail rather than silently act as input.
   try{this.power.set({throttle:.5});this.state.unknownFieldBlocked=false;}
   catch(error){this.state.unknownFieldBlocked=error instanceof TypeError;}
   this.state.positiveCommand=this.power.set({sources:[{name:'engine',input:.5,starter:1}],links:[{name:'clutch',engagement:.5}],brakes:[{name:'left',engagement:.2}]});
   this.power.enabled=false;
  }else if(this.state.phase===1){
   this.state.disabled=this.power.enabled===false&&!this.power.state.available;
   this.power.enabled=true;
  }else if(this.state.phase===2){
   const state=this.power.state;
   this.state.measuredState=state.available&&state.shafts.length===5&&state.shafts.every(s=>Number.isFinite(s.rpm))&&state.sources.some(s=>s.torque>0);
   world.entity('100').destroy();this.state.staleInvalid=!this.power.valid;
   try{void this.power.state;this.state.staleReadBlocked=false;}
   catch(error){this.state.staleReadBlocked=error instanceof ReferenceError;}
   try{this.power.set({sources:[{name:'engine',input:1}]});this.state.staleWriteBlocked=false;}
   catch(error){this.state.staleWriteBlocked=error instanceof ReferenceError;}
   this.state.finished=true;
  }
  ++this.state.phase;
 }
}
