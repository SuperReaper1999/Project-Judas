// Real native-WASM player, actual browser input and independently read rendered pixels.
// PLAYWRIGHT_MODULE=... node player.cjs PACKAGE FRESH_OUTPUT chromium|firefox [DPR]
const {chromium,firefox}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('fs'),path=require('path'),{spawn,spawnSync}=require('child_process');
const [pkg,out,name='chromium',dpr='1']=process.argv.slice(2),port=8798;
fs.mkdirSync(out);const rows=[],checks=[],sizes=[];let context,server;
const wait=ms=>new Promise(r=>setTimeout(r,ms));
function check(v,m){if(!v)throw Error(m);checks.push(m);console.log('PASS',m);}
(async()=>{
 server=spawn('python3',[path.resolve(pkg,'serve.py'),path.resolve(pkg),'--port',String(port)],{cwd:path.resolve(out)});
 server.stderr.on('data',b=>rows.push(String(b)));
 await new Promise((r,j)=>{server.stdout.once('data',r);server.once('exit',c=>j(Error('server '+c)));});
 const type=name==='firefox'?firefox:chromium;
 async function open(){
  context=await type.launchPersistentContext(path.resolve(out,'profile'),{headless:!process.env.JUDAS_HEADED,
   ...(name==='chromium'?{channel:'chromium'}:{}),
   viewport:{width:1280,height:960},deviceScaleFactor:Number(dpr),
   args:name==='chromium'?['--no-sandbox','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader','--disable-dev-shm-usage']:[]});
  const p=await context.newPage();p.latest=null;p.errors=[];
  p.on('console',m=>{const s=m.text();rows.push(s);if(s.includes('PLAYER_PROBE '))try{p.latest=JSON.parse(s.split('PLAYER_PROBE ')[1]);}catch(e){p.errors.push(e.message);}});
  p.on('pageerror',e=>p.errors.push(e.stack));
  await p.goto('http://127.0.0.1:'+port,{timeout:120000});
  await p.waitForFunction(()=>['Play','Unavailable'].includes(document.querySelector('#play').textContent),null,{timeout:120000});
  check(await p.locator('#play').innerText()==='Play','preload ready');await p.locator('#play').click();
  await until(p,r=>r.supported,'motor lands on real floor');return p;
 }
 async function until(p,predicate,label,timeout=30000){const start=Date.now();while(Date.now()-start<timeout){if(p.errors.length)throw Error(p.errors.join('\n'));if(p.latest&&predicate(p.latest))return p.latest;await wait(40);}throw Error(label+': '+JSON.stringify(p.latest));}
 async function tap(p,key){await p.keyboard.down(key);await wait(100);await p.keyboard.up(key);}
 let p=await open();
 // Browser refusal/loss is independent of the game's request. Ordinary page
 // movement must not rotate a player while the request is still true.
 await p.evaluate(()=>document.exitPointerLock?.());let unlockedBoundary=p.latest.sequence;
 await until(p,r=>r.sequence>=unlockedBoundary+10,'lock release settles');const unlockedYaw=p.latest.state.yaw;
 unlockedBoundary=p.latest.sequence;await p.mouse.move(600,400);
 await until(p,r=>r.sequence>=unlockedBoundary+10,'unlocked page motion settles');
 rows.push('UNLOCKED '+JSON.stringify({before:unlockedYaw,after:p.latest.state.yaw,locked:await p.evaluate(()=>!!document.pointerLockElement)}));
 check(p.latest.state.captured&&Math.abs(p.latest.state.yaw-unlockedYaw)<1e-7,'ungranted pointer capture cannot rotate camera');

 async function dimensions(){return p.evaluate(()=>{const c=document.querySelector('#canvas'),r=c.getBoundingClientRect();return {width:Module.judasSurface.width,height:Module.judasSurface.height,dom:{width:c.width,height:c.height},css:{width:r.width,height:r.height},ratio:devicePixelRatio,surface:Module.judasSurface};});}
 // Each marker is drawn by Judas RuntimeUI, not by the HTML shell or telemetry.
 async function pixels(label){
  const file=path.join(out,label+'.png');const rect=await p.locator('#canvas').boundingBox();
  await wait(80);const settled=await p.locator('#canvas').boundingBox();
  check(JSON.stringify(rect)===JSON.stringify(settled),label+' stable CSS rectangle');
  await p.screenshot({path:file,clip:rect});
  const result=spawnSync('python3',['-c',`from PIL import Image
import json,sys
im=Image.open(sys.argv[1]).convert('RGB');w,h=im.size;s=min(w/1280,h/720);ox=(w-1280*s)/2;oy=(h-720*s)/2
points=[(ox+12*s,oy+12*s),(ox+1268*s,oy+12*s),(ox+12*s,oy+708*s),(ox+1268*s,oy+708*s)]
colors=[im.getpixel((int(x),int(y))) for x,y in points]
print(json.dumps(colors))`,file],{encoding:'utf8'});
  if(result.status!==0)throw Error(result.stderr);const colors=JSON.parse(result.stdout);
  check(colors.every((c,i)=>[0,1,2].every(k=>Math.abs(c[k]-[[255,0,0],[0,255,0],[0,0,255],[255,255,0]][i][k])<12)),label+' actual rendered corner markers');
 }
 async function settledSurface(){
  await p.waitForFunction(()=>{const c=document.querySelector('#canvas'),r=c.getBoundingClientRect();
   const s=Math.min(devicePixelRatio,2,4096/r.width,4096/r.height,Math.sqrt(2097152/r.width/r.height));
   return Module.judasSurface?.width===Math.max(1,Math.floor(r.width*s))&&Module.judasSurface?.height===Math.max(1,Math.floor(r.height*s));},null,{timeout:20000});
  const d=await dimensions();rows.push('SURFACE '+JSON.stringify(d));
  const ack=await until(p,r=>r.viewport.width===d.width&&r.viewport.height===d.height,'public viewport matches backing surface');
  await until(p,r=>r.sequence>ack.sequence,'completed render after viewport publication');return d;
 }
 for(const [w,h] of [[1280,960],[700,1000],[1441,761],[980,700]]){
  await p.setViewportSize({width:w,height:h});const d=await settledSurface();
  check(d.width*d.height<=2*1024*1024&&d.width<=4096&&d.height<=4096,'bounded drawable '+w+'x'+h);
  check(Math.abs(d.width/d.height-d.css.width/d.css.height)<.006,'aspect preserved '+w+'x'+h);
  sizes.push(d);await pixels(w+'x'+h);
 }
 check(p.latest.state.time>0&&p.latest.supported,'actual MK2 motor and gameplay progress');
 const x=p.latest.position.x;await p.locator('#canvas').click({position:{x:100,y:100}});
 await p.waitForFunction(()=>document.pointerLockElement===document.querySelector('#canvas'));
 let boundary=p.latest.sequence;
 await until(p,r=>r.sequence>=boundary+10,'native capture acquisition boundary');
 const initialYaw=p.latest.state.yaw;await p.mouse.move(300,300);
 await until(p,r=>Math.abs(r.state.yaw-initialYaw)>.02,'first real mouse event acknowledged');
 boundary=p.latest.sequence;await until(p,r=>r.sequence>=boundary+10,'first real mouse event settles');const yaw=p.latest.state.yaw;
 await p.evaluate(()=>{window.testMouse=[];document.querySelector('#canvas').addEventListener('mousemove',e=>window.testMouse.push(e.movementX));});
 boundary=p.latest.sequence;await p.mouse.move(340,300);
 await until(p,r=>r.sequence>=boundary+10,'measured real mouse event settles');
 const yawDelta=p.latest.state.yaw-yaw,domDelta=await p.evaluate(()=>testMouse.reduce((a,b)=>a+b,0));
 rows.push('MOUSE '+JSON.stringify({yawDelta,domDelta}));
 check(Math.abs(domDelta)>0&&Math.abs(yawDelta+domDelta*.0021)<.01,'mouse look uses CSS pixels independent of backing DPI');

 await p.keyboard.down('KeyD');await until(p,r=>r.position.x>x+.6,'real keyboard moves motor');await p.keyboard.up('KeyD');
 await until(p,r=>Math.abs(r.velocity.x)<.01,'released movement settles');
 // A page control must not steer the game through SDL's document listener.
 await p.evaluate(()=>{document.exitPointerLock?.();document.querySelector('#fullscreen').focus();});await wait(300);
 const blurred=p.latest.position.x;await p.keyboard.down('KeyD');await wait(500);await p.keyboard.up('KeyD');await wait(300);
 check(Math.abs(p.latest.position.x-blurred)<.03,'page control focus cannot move player');
 await p.locator('#canvas').click({position:{x:100,y:100}});await wait(300);
 check(Math.abs(p.latest.velocity.x)<.01,'refocus has no stuck held movement');
 await p.keyboard.down('Space');await until(p,r=>r.position.y>1.3,'jump leaves physical floor');await p.keyboard.up('Space');await until(p,r=>r.supported,'jump lands');
 await tap(p,'Escape');await until(p,r=>!r.state.captured,'release mouse for UI');
 // The same real UI button must hit at the layout's physical location after resize/DPI.
 const box=await p.locator('#canvas').boundingBox();const s=Math.min(box.width/1280,box.height/720);
 const ox=(box.width-1280*s)/2,oy=(box.height-720*s)/2,clicks=p.latest.state.clicks;
 await p.mouse.click(box.x+ox+1143*s,box.y+oy+664*s);await until(p,r=>r.state.clicks===clicks+1,'actual resized UI hit');check(true,'resized UI click reaches owning JudasJS script');
 // Firefox reports an emulated screen to page JS but uses its native
 // headless screen for fullscreen. Calibrate that size, exit, then align the
 // capture viewport BEFORE the actual fullscreen pixel/button qualification.
 if(name==='firefox'){
  await p.locator('#fullscreen').click();await p.waitForFunction(()=>!!document.fullscreenElement);
  const nativeScreen=await p.evaluate(()=>({width:innerWidth,height:innerHeight}));
  await p.evaluate(()=>document.exitFullscreen());await p.waitForFunction(()=>!document.fullscreenElement);
  await p.setViewportSize(nativeScreen);await settledSurface();
 }
 await p.locator('#fullscreen').click();await p.waitForFunction(()=>!!document.fullscreenElement);
 const full=await settledSurface();
 await pixels('fullscreen');
 check(full.surface.active&&await p.evaluate(()=>document.fullscreenElement===document.querySelector('#viewport')),'real Fullscreen API entered');await p.locator('#exit-fullscreen').click();await p.waitForFunction(()=>!document.fullscreenElement);
 if(name==='firefox')await p.setViewportSize({width:980,height:700});await settledSurface();
 await p.locator('#canvas').click({position:{x:100,y:100}});await tap(p,'KeyK');
 const saved=await until(p,r=>r.status?.operation==='save'&&r.status.state==='completed','durable save');
 await context.close();context=null;p=await open();await tap(p,'KeyL');
 const loaded=await until(p,r=>r.restored&&!r.busy,'fresh browser process load');
 check(Math.hypot(loaded.position.x-saved.position.x,loaded.position.z-saved.position.z)<.02&&loaded.state.clicks===saved.state.clicks,'cold load restores physical endpoint and UI progress');
 check(!p.errors.length,'no runtime exceptions');
 fs.writeFileSync(path.join(out,'RESULTS.json'),JSON.stringify({passed:true,browser:name,version:context.browser().version(),dpr:Number(dpr),checks,sizes,yawDelta,visibility:"separate visibility.cjs normal-browser gate",saved:saved.position,loaded:loaded.position},null,2));
})().catch(e=>{console.error(e.stack);process.exitCode=1;}).finally(async()=>{fs.writeFileSync(path.join(out,'console.log'),rows.join('\n'));if(context)await context.close();if(server)server.kill('SIGTERM');});
