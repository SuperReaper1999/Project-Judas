import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const source=fs.readFileSync(new URL('../tools/crash_proof/DrivingProject.js',import.meta.url),'utf8').replace('export default class','class Driver')+'\nglobalThis.Driver=Driver;';
const entities=new Map(),impulses=[],views=[],pressed=new Set(),held=new Set(),axes=new Map(),events=[];
const rigs=Array.from({length:4},(_,n)=>({owner:String(n+1),reference:0,wheels:[],brakes:[],panels:['panel-'+n],label:'Lane '+(n+1)}));
for(let n=0;n<4;n++){
 const parts=[{key:'root',index:0,component:0,entity:'body-'+n,mass:1000},{key:'front',index:1,component:0,entity:'front-'+n,mass:400},{key:'debris',index:2,component:2,entity:'debris-'+n,mass:50}];
 entities.set(String(n+1),{fracture:{state:{parts,interfaces:[]},impulse:(part,p)=>impulses.push({owner:n+1,part,p})}});
 entities.set('body-'+n,{velocity:{x:0,y:0,z:0},transform:{rotation:{w:1,x:0,y:0,z:0}},presentedTransform:{position:{x:n*12,y:1,z:0},rotation:{w:1,x:0,y:0,z:0}}});
 entities.set('panel-'+n,{deformable:{state:{mass:25,attachments:[{active:true,released:false,part:'root'}]},impulse:p=>impulses.push({panel:n,p})}});
}
let text='';const hud={modal:false,get:()=>({set text(v){text=v;}})};const input={pressed:k=>pressed.has(k),held:k=>held.has(k),axis:k=>axes.get(k)||0,pointerCapture:false};
const context={rigs,world:{entity:id=>entities.get(String(id)),setView:v=>views.push(v),clearView(){}},input,ui:{get:()=>hud},saves:{busy:false,save(){},load(){},status:()=>({state:'complete'})},scenes:{reload(){}},console:{log:line=>events.push(line)}};
for(let n=0;n<4;n++)for(const p of entities.get(String(n+1)).fracture.state.parts)if(p.entity)p.entity=entities.get(p.entity)||{id:p.entity};
vm.runInNewContext(source,context);const driver=new context.Driver();driver.start();driver.presentationUpdate(1/60);
pressed.add('select_4');driver.uiUpdate();pressed.clear();driver.presentationUpdate(1/60);
assert.equal(driver.state.selected,3);assert.match(text,/CAR 4 \/ 4/);assert.ok(views.at(-1).position.x>30,'selection must move camera to the fourth physical vehicle');
const rotation=JSON.stringify(views.at(-1).rotation);entities.get('body-3').presentedTransform.rotation={w:.70710678,x:0,y:0,z:.70710678};entities.get('body-3').presentedTransform.position.y+=.3;driver.presentationUpdate(1/60);
assert.equal(JSON.stringify(views.at(-1).rotation),rotation,'chassis roll must not roll the camera');assert.ok(Math.abs(driver.focus.y-1.45)<.04,'suspension bob must be filtered by the camera only');
held.add('orbit');axes.set('look_x',50);axes.set('look_y',-20);driver.uiUpdate();driver.presentationUpdate(1/60);assert.ok(input.pointerCapture);assert.notEqual(JSON.stringify(views.at(-1).rotation),rotation,'mouse orbit must change view');held.clear();axes.clear();driver.uiUpdate();assert.ok(!input.pointerCapture);
pressed.add('launch');driver.fixedUpdate();pressed.clear();assert.equal(impulses.length,3);assert.deepEqual(impulses.filter(x=>x.owner).map(x=>x.part),[0,1]);assert.ok(impulses.every(x=>x.owner===4||x.panel===3),'launch only affects the selected car and its connected panel');assert.equal(impulses[0].p.z,-25000);assert.equal(impulses.at(-1).p.z,-625);
impulses.length=0;entities.get('panel-3').deformable.state.attachments[0].released=true;pressed.add('launch');driver.fixedUpdate();pressed.clear();assert.equal(impulses.length,2,'detached bonnet must not be relaunched with the car');
// A six-wheel layout and its authored forward axis must use the same controls.
rigs[3].forward={x:0,y:0,z:1};rigs[3].steered=[0,1];const wheelSets=[],powerSets=[];
rigs[3].wheels=Array.from({length:6},(_,i)=>'wheel-'+i);rigs[3].brakes=Array(6).fill(null);
for(const id of rigs[3].wheels)entities.set(id,{wheel:{set:s=>wheelSets.push({id,...s})}});
entities.get('4').powertrain={settings:{sources:[{name:'front'},{name:'rear'}],brakes:[{name:'brake'}]},set:s=>powerSets.push(s)};
axes.set('drive',.5);axes.set('steer',1);driver.fixedUpdate();axes.clear();
assert.equal(wheelSets.length,6);assert.equal(wheelSets.filter(s=>s.steeringAngle===.4).length,2);
assert.equal(powerSets[0].sources.length,2);assert.ok(powerSets[0].sources.every(s=>s.input===.5));
impulses.length=0;driver.launch();assert.ok(impulses[0].p.z>0,'author-defined forward axis governs a six-wheel truck launch');
rigs.splice(1);driver.state.selected=0;driver.uiUpdate();assert.match(text,/One authored car/);assert.doesNotMatch(text,/1-4 select/);
driver.destroy();assert.ok(!input.pointerCapture);console.log('Crash driving project: switching, truthful HUD, camera isolation/orbit, physical launch and detached-part exclusion passed.');
