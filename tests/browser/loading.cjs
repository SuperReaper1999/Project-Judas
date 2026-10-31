// Exercise the actual shell callbacks, without downloading another WASM runtime.
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync(process.argv[2]||path.join(__dirname,'../../platform/browser/shell.html'),'utf8');
const script=[...html.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/g)].map(m=>m[1]).find(s=>s.includes('const statusText='));
let checks=0;
function check(v,m){assert(v,m);checks++;}
function setup(){
 const elements={};
 for(const id of ['status','play','canvas','loading','fullscreen','viewport','exit-fullscreen'])elements['#'+id]={hidden:false,disabled:true,textContent:'',addEventListener(){},focus(){}};
 const document={querySelector:id=>elements[id],addEventListener(){}};
 const context={document,window:{addEventListener(){}},console,JUDAS_GAME:{}};
 vm.createContext(context);vm.runInContext(script,context);
 return {module:context.Module,elements};
}
check(/<progress[^>]+id="loading"[^>]*>/.test(html),'loading indicator is present');
check(!/<progress[^>]+(?:value|max)=/.test(html),'unknown network progress remains indeterminate');
let {module:mod,elements:e}=setup();
mod.setStatus('Downloading data... (1048576/100)');
check(e['#status'].textContent==='Downloading game content…','compressed lengths cannot produce a false ratio');
mod.setStatus('Preparing... (1/3)');
check(e['#status'].textContent==='Preparing...','internal dependency counts are not download percentages');
mod.onRuntimeInitialized();
check(e['#loading'].hidden&&e['#play'].textContent==='Play'&&!e['#play'].disabled,'ready hides progress and enables play');
mod.setStatus('Downloading data... (1/2)');
check(e['#status'].textContent.startsWith('Ready.'),'late status cannot overwrite readiness');
({module:mod,elements:e}=setup());
mod.onAbort('network failure');
check(e['#loading'].hidden&&e['#play'].disabled&&e['#play'].textContent==='Unavailable','failure stops loading affordance');
mod.onRuntimeInitialized();mod.setStatus('Downloading data... (1/2)');
check(e['#status'].textContent==='Game stopped: network failure','failure cannot turn into false readiness');
console.log(checks+' shell loading checks passed');
