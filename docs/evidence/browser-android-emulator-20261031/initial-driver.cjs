const {chromium}=require(process.env.PLAYWRIGHT_MODULE),fs=require('fs'),path=require('path'),crypto=require('crypto'),{execFileSync}=require('child_process');
const root=path.resolve('.cache/browser-emulator'),c=JSON.parse(fs.readFileSync(root+'/CONNECTION.json')),out=root+'/run';fs.mkdirSync(out);
const raw=fs.createWriteStream(out+'/game-console.log'),checks=[],observations={};let p,session,latest,phase='start';
const wait=ms=>new Promise(r=>setTimeout(r,ms));
const adb=(...args)=>execFileSync(c.adb,['-s',c.device,...args],{encoding:'utf8',timeout:15000}).trim();
function save(){fs.writeFileSync(out+'/RESULTS.json',JSON.stringify({phase,checks,observations,latest},null,2))}
function check(value,message){if(!value)throw Error(message);checks.push(message);console.log('PASS',message);save()}
async function until(fn,label,ms=30000){const deadline=Date.now()+ms;while(Date.now()<deadline){if(fn())return;await wait(80)}throw Error(label+': '+JSON.stringify(latest))}
async function attach(){
 const b=await chromium.connectOverCDP('http://127.0.0.1:'+c.debugPort,{timeout:15000});
 p=b.contexts().flatMap(x=>x.pages()).find(x=>x.url().startsWith(c.url));if(!p)throw Error('Own emulator tab absent');
 session=await p.context().newCDPSession(p);latest=null;
 p.on('console',m=>{const s=m.text();raw.write(phase+' '+s+'\n');if(s.includes('EMULATOR_BREAKWATER '))try{latest=JSON.parse(s.split('EMULATOR_BREAKWATER ')[1])}catch{}});
 p.on('pageerror',e=>{raw.write('PAGEERROR '+e+'\n');observations.pageErrors=(observations.pageErrors||[]).concat(String(e));save()});
 await p.waitForFunction(()=>['Play','Unavailable','Playing'].includes(document.querySelector('#play').textContent),null,{timeout:60000,polling:100});
 check(await p.locator('#play').innerText()!=='Unavailable','emulator browser preloads actual WASM/game/storage');
 if(await p.locator('#play').innerText()==='Play'){
  const r=await p.locator('#play').boundingBox();await touch(r.x+r.width/2,r.y+r.height/2);
 }
 await until(()=>latest?.paddle&&latest.balls?.length,'native game observes real physical entities');
}
async function touch(x,y,endX=x,endY=y){
 await session.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y,id:0,force:1,radiusX:1,radiusY:1}]});
 for(let n=1;n<=8;n++){await session.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:x+(endX-x)*n/8,y:y+(endY-y)*n/8,id:0,force:1,radiusX:1,radiusY:1}]});await wait(30)}
 await session.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await wait(100);
}
const ui=JSON.parse(fs.readFileSync('projects/breakwater/Assets/ui/hud.judasui')).data;
async function tapUI(id){
 await p.locator('#canvas').scrollIntoViewIfNeeded();const r=await p.locator('#canvas').boundingBox(),s=Math.min(r.width/720,r.height/1280),ox=(r.width-720*s)/2,oy=(r.height-1280*s)/2;
 const e=ui.elements.find(e=>e.id===id),x=e.anchorMin[0]*720+e.offset[0]+(.5-e.align[0])*e.size[0],y=e.anchorMin[1]*1280+e.offset[1]+(.5-e.align[1])*e.size[1];
 await touch(r.x+ox+x*s,r.y+oy+y*s);
}
async function bytes(){return p.evaluate(()=>{
 const root='/persistent/judas/games/'+JUDAS_GAME.saveIdentity;let found=null;
 function walk(d,n){if(n>8)return;for(const name of Module.FS.readdir(d)){if(name==='.'||name==='..')continue;const q=d+'/'+name;if(Module.FS.isDir(Module.FS.stat(q).mode))walk(q,n+1);else if(name==='breakwater.save')found=Array.from(Module.FS.readFile(q));}}
 walk(root,0);return found;
})}
const digest=a=>crypto.createHash('sha256').update(Buffer.from(a)).digest('hex');
(async()=>{
 phase='boot';await attach();observations.environment=await p.evaluate(()=>({ua:navigator.userAgent,isolated:crossOriginIsolated,width:innerWidth,height:innerHeight,dpr:devicePixelRatio,surface:Module.judasSurface}));
 check(observations.environment.isolated,'emulator localhost has required isolation');
 phase='touch';await p.locator('#canvas').scrollIntoViewIfNeeded();let r=await p.locator('#canvas').boundingBox();
 const y=r.y+Math.min(r.height*.65,Math.max(50,r.height-50));await touch(r.x+r.width*.5,y,r.x+r.width*.82,y);await until(()=>latest.paddle.x>.3,'touch moves physical paddle');
 const saved=JSON.parse(JSON.stringify(latest));observations.savedEndpoint=saved;check(saved.paddle.x>.3,'touch input changes real kinematic paddle pose');
 phase='save';await tapUI('pause');await until(()=>latest.menu,'pause menu opens');await tapUI('save');await until(()=>latest.message==='Game saved on this device','durable save finishes');
 let data=await bytes();check(data?.length>100,'actual binary save published');observations.saveBytes=data.length;observations.saveSha256=digest(data);fs.writeFileSync(out+'/breakwater.save',Buffer.from(data));
 r=await p.locator('#canvas').boundingBox();await touch(r.x+r.width*.5,r.y+r.height*.65,r.x+r.width*.15,r.y+r.height*.65);await until(()=>latest.paddle.x<-.3,'post-save physical divergence');check(latest.paddle.x<-.3,'negative control differs physically from saved position');
 phase='cold';await p.goto('about:blank');await session.detach();adb('shell','am','force-stop','com.android.chrome');adb('shell','am','start','-a','android.intent.action.VIEW','-d',c.url,'-p','com.android.chrome');await wait(2000);await attach();
 data=await bytes();check(data&&digest(data)===observations.saveSha256,'fresh Chrome process hydrates exact IndexedDB save bytes');check(Math.abs(latest.paddle.x-saved.paddle.x)>.2,'fresh gameplay has not falsely restored cached state');
 await tapUI('pause');await until(()=>latest.menu,'new pause menu');await tapUI('load');await until(()=>latest.message==='Saved game restored','real load completes');
 check(Math.abs(latest.paddle.x-saved.paddle.x)<.02,'cold load restores actual physical paddle endpoint');observations.restored=JSON.parse(JSON.stringify(latest));
 phase='audio';await p.evaluate(()=>{
 const d=window.miniaudio?.devices.find(d=>d?.webaudio);if(!d)throw Error('No native mixer Web Audio device');
 const a=d.webaudio.createAnalyser(),g=d.webaudio.createGain();g.gain.value=0;d.scriptNode.connect(a);a.connect(g);g.connect(d.webaudio.destination);
 window.qualAudio={a,context:d.webaudio,states:[],peak:0};
 d.webaudio.addEventListener('statechange',()=>qualAudio.states.push({state:d.webaudio.state,time:Date.now()}));
 window.qualVisibility=[];document.addEventListener('visibilitychange',()=>qualVisibility.push({hidden:document.hidden,epoch:Module.judasVisibilityEpoch,time:Date.now()}));
 window.qualAudio.timer=setInterval(()=>{const x=new Float32Array(a.fftSize);a.getFloatTimeDomainData(x);qualAudio.peak=Math.max(qualAudio.peak,Math.sqrt(x.reduce((s,v)=>s+v*v,0)/x.length))},20);
 });
 await tapUI('launch');await until(()=>latest.state.phase==='playing'&&latest.balls.some(b=>Math.hypot(b.velocity.x,b.velocity.y)>1),'physical ball launched');await wait(1500);
 observations.audio=await p.evaluate(()=>({state:qualAudio.context.state,peak:qualAudio.peak}));check(observations.audio.state==='running'&&observations.audio.peak>1e-5,'actual native mixer PCM reaches Web Audio graph');
 phase='rotate';const before=latest.viewport;adb('shell','settings','put','system','accelerometer_rotation','0');adb('shell','settings','put','system','user_rotation','1');
 await until(()=>latest.viewport.width>latest.viewport.height,'landscape native viewport');observations.landscape={viewport:latest.viewport,surface:await p.evaluate(()=>Module.judasSurface)};check(latest.viewport.width!==before.width,'rotation publishes real renderer/gameplay viewport');await p.screenshot({path:out+'/landscape.png'});
 adb('shell','settings','put','system','user_rotation','0');await until(()=>latest.viewport.height>latest.viewport.width,'portrait returns');check(true,'portrait/landscape roundtrip preserves running game');
 phase='home';await wait(200);let beforeHome=JSON.parse(JSON.stringify(latest));adb('shell','input','keyevent','KEYCODE_HOME');await wait(4000);phase='resume';let first=null;
 const record=m=>{const s=m.text();if(!first&&s.includes('EMULATOR_BREAKWATER '))try{first=JSON.parse(s.split('EMULATOR_BREAKWATER ')[1])}catch{}};p.on('console',record);
 adb('shell','am','start','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER','-p','com.android.chrome');await until(()=>first,'first resume state');p.off('console',record);
 observations.home={before:beforeHome.state.time,after:first.state.time,delta:first.state.time-beforeHome.state.time};check(observations.home.delta<.5,'Home/resume does not simulate four seconds of hidden time');
 phase='lock';beforeHome=JSON.parse(JSON.stringify(latest));adb('shell','input','keyevent','KEYCODE_SLEEP');await wait(3000);adb('shell','input','keyevent','KEYCODE_WAKEUP');adb('shell','wm','dismiss-keyguard');await wait(500);
 check(await p.evaluate(()=>!document.hidden),'emulated lock/unlock returns visible game');observations.audioLifecycle=await p.evaluate(()=>({state:qualAudio.context.state,states:qualAudio.states,visibility:qualVisibility}));
 check(observations.audioLifecycle.states.some(s=>s.state==='suspended'),'native audio context suspends during background/lock');
 if(observations.audioLifecycle.state!=='running'){r=await p.locator('#canvas').boundingBox();await touch(r.x+r.width/2,r.y+r.height/2);await wait(300);observations.audioLifecycle.resumeRequiredGesture=true;observations.audioLifecycle.state=await p.evaluate(()=>qualAudio.context.state)}
 check(observations.audioLifecycle.state==='running','audio context resumes after return or explicit gesture');
 check(!observations.pageErrors?.length,'no observed browser runtime errors');phase='complete';save();console.log('EMULATOR QUALIFICATION PASSED');await session.detach();raw.end(()=>process.exit(0));
})().catch(async e=>{observations.failure=String(e);save();console.error(e);try{if(p)await p.screenshot({path:out+'/failure.png',timeout:5000});if(session)await session.detach()}catch{}raw.end(()=>process.exit(1))});
