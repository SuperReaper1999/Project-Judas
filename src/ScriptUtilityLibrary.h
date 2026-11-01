#pragma once
// Appended to the built-in Judas module, sharing its private call helper.
constexpr const char* kUtilityAILibrary = R"JS(
export class UtilityAgent {
 #handlers; #handle; #selected; #context; #closed; #busy; #resume; #phase; #outcome;
 constructor(definition,handlers={},name='NPC AI') {
  this.#handlers=handlers; this.#selected=null; this.#context={}; this.#closed=false;
  this.#busy=false; this.#resume=false;
  this.#handle=call('aiCreate',JSON.stringify(definition),name,()=>this.#retire());
 }
 #check(){if(this.#closed)throw new Error('AI agent is disposed');}
 #invoke(method,selection,...args){
  const previousPhase=this.#phase;this.#phase=method;
  try { const fn=this.#handlers[selection.action]?.[method];
  if(fn!==undefined){if(typeof fn!=='function')throw new TypeError('AI handler must be a function');
   const result=fn(...args);if(result&&typeof result.then==='function')throw new TypeError('AI actions must be synchronous');}
  } finally {this.#phase=previousPhase;}
 }
 #retire(){if(this.#closed)return;this.#closed=true;const old=this.#selected;this.#selected=null;
  if(old)this.#invoke('stop',old,this.#context,old,'disposed');}
 get inspection(){this.#check();return call('aiInspect',this.#handle);}
 tick(dt,candidates,context={}) {
  this.#check();if(this.#busy)throw new Error('AI tick cannot be reentered');
  this.#busy=true;this.#context=context;this.#outcome=null;
  try {
   const due=call('aiDue',this.#handle,dt);
   const values=due?(typeof candidates==='function'?candidates(context):candidates):[];
   const result=call('aiTick',this.#handle,dt,JSON.stringify(values));
   if(this.#resume||result.changed){
    const old=this.#selected;this.#selected=null;
    if(old)this.#invoke('stop',old,context,old,result.reason);
    if(this.#closed){result.selected=null;result.status='disposed';result.reason='agent disposed';result.changed=true;return result;}
    this.#selected=result.selected?Object.freeze({...result.selected}):null;this.#resume=false;
    if(this.#selected)this.#invoke('start',this.#selected,context,this.#selected);
   }
   if(this.#selected)this.#invoke('update',this.#selected,dt,context,this.#selected);
   if(this.#outcome)Object.assign(result,this.#outcome);
   if(this.#closed){result.status='disposed';result.reason='agent disposed';result.changed=true;}
   result.selected=this.#selected?{...this.#selected}:null;
   return result;
  } catch(error) {
   if(!this.#closed){call('aiFinish',this.#handle,'failure');const old=this.#selected;this.#selected=null;
    if(old){try{this.#invoke('stop',old,context,old,'failure');}catch(cleanup){console.log('AI cleanup failed: '+cleanup);}}}
   throw error;
  } finally {this.#busy=false;}
 }
 finish(status='success',context=this.#context){this.#check();if(this.#phase==='stop'||this.#phase==='start')throw new Error('Finish from update, not start/stop');call('aiFinish',this.#handle,status);
  if(this.#busy)this.#outcome=call('aiInspect',this.#handle);
  const old=this.#selected;this.#selected=null;if(old)this.#invoke('stop',old,context,old,status);}
 snapshot(){this.#check();return call('aiSnapshot',this.#handle);}
 restore(snapshot){this.#check();if(this.#busy)throw new Error('Cannot restore during AI action execution');
  call('aiRestore',this.#handle,JSON.stringify(snapshot));
  const old=this.#selected;this.#selected=null;this.#resume=true;
  if(old)this.#invoke('stop',old,this.#context,old,'restored');}
 dispose(){if(!this.#closed)call('aiDispose',this.#handle);}
}
)JS";
