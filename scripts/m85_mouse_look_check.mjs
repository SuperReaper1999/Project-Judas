// Focused project-script regression; no native input/physics implementation mocked
// as verified. Only logical input values are supplied to the actual demo class.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const source=fs.readFileSync(new URL('../projects/planetary_terrain_lab/Assets/scripts/explore.js',import.meta.url),'utf8');
const mathSource=fs.readFileSync(new URL('../projects/planetary_terrain_lab/Assets/scripts/math.js',import.meta.url),'utf8');
const math=await import('data:text/javascript;base64,'+Buffer.from(mathSource).toString('base64'));
const names=['world','input','ui','scenes','saves',...Object.keys(math)];
const load=new Function(...names,source.replace(/^import .*;\r?\n/gm,'').replace('export default class','return class'));
let checks=0,failed=0;
function check(value,label){checks++;try{assert(value);console.log('PASS',label);}catch{failed++;console.log('FAIL',label);}}
function controller(up,deltaX,deltaY=0){
 const input={axis:name=>name==='look_x'?deltaX:name==='look_y'?deltaY:0,pressed:()=>false,held:()=>false};
 const Class=load({},input,{},{},{},...Object.values(math));const instance=new Class();
 instance.hud={modal:false};instance.state.mode='ground';instance.state.direction=up;return instance;
}
for(const up of [{x:0,y:1,z:0},{x:1,y:0,z:0},math.norm({x:1,y:2,z:3})]){
 const right=controller(up,100),left=controller(up,-100);const start=right.basis();right.update(1/60);left.update(1/60);
 check(math.dot(right.basis().forward,start.right)>0,'positive logical horizontal mouse turns toward camera right');
 check(math.dot(left.basis().forward,start.right)<0,'negative logical horizontal mouse turns toward camera left');
 check(Math.abs(math.dot(right.basis().forward,up))<1e-12,'horizontal look respects current arbitrary gravity tangent');
 const paused=controller(up,100);paused.hud.modal=true;paused.update(1/60);check(paused.state.yaw===0,'modal UI blocks horizontal look');
 const vertical=controller(up,0,100);vertical.update(1/60);check(vertical.state.pitch<0&&vertical.state.yaw===0,'existing vertical-look direction is preserved');
 const idle=controller(up,0);idle.update(1/60);check(idle.state.yaw===0,'independent idle instance retains its yaw');
}
console.log(`SUMMARY ${checks} checks ${failed} failures`);process.exitCode=Number(failed>0);
