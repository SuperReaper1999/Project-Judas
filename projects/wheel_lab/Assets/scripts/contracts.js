// Executed by the focused application fixture, never the normal playable lab.
import {world} from 'judas';
export default class {
 constructor(){this.state={phase:0};this.wheel=null;}
 start(){
  this.wheel=world.entity('101').wheel;
  this.state.valid=this.wheel.valid&&world.entity('100').wheel===null;
  try{this.wheel.set({driveTorque:1});this.state.startMutationBlocked=false;}
  catch(error){this.state.startMutationBlocked=error instanceof TypeError;}
 }
 fixedUpdate(){
  if(this.state.phase===0){
   const before=this.wheel.settings.friction;
   try{this.wheel.configure({friction:-1});this.state.invalidConfigBlocked=false;}
   catch(error){this.state.invalidConfigBlocked=error instanceof TypeError&&this.wheel.settings.friction===before;}
   try{this.wheel.set({driveTorque:10,brakeTorque:-1});this.state.invalidCommandBlocked=false;}
   catch(error){this.state.invalidCommandBlocked=error instanceof TypeError;}
   // @ts-expect-error Deliberately invalid key proves runtime rejects unknown fields.
   try{this.wheel.configure({fiction:1});this.state.unknownFieldBlocked=false;}
   catch(error){this.state.unknownFieldBlocked=error instanceof TypeError;}
   this.state.positiveConfig=this.wheel.configure({friction:.64})&&Math.abs(this.wheel.settings.friction-.64)<1e-6;
   this.wheel.enabled=false;
  }else if(this.state.phase===1){
   const state=this.wheel.state;
   this.state.disabled=this.wheel.enabled===false&&!state.contact&&state.normalForce===0;
   this.wheel.enabled=true;this.state.positiveCommand=this.wheel.set({driveTorque:80,brakeTorque:0,steeringAngle:.1});
  }else if(this.state.phase===2){
   this.state.contacts=this.wheel.state.available;
   world.entity('101').destroy();this.state.staleInvalid=!this.wheel.valid;
   try{void this.wheel.state;this.state.staleReadBlocked=false;}
   catch(error){this.state.staleReadBlocked=error instanceof ReferenceError;}
   try{this.wheel.set({driveTorque:1});this.state.staleWriteBlocked=false;}
   catch(error){this.state.staleWriteBlocked=error instanceof ReferenceError;}
   this.state.finished=true;
  }
  ++this.state.phase;
 }
}
