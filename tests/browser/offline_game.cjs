// Genuine game input and fresh-process persistence. Requires Playwright browsers.
// PLAYWRIGHT_MODULE=/absolute/node_modules/playwright node this.cjs PACKAGE OUT chromium|firefox
const {chromium,firefox}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('fs'),path=require('path'),crypto=require('crypto'),{spawn}=require('child_process');
const [pkg,out,name='chromium']=process.argv.slice(2),port=8796;
if(!pkg||!out||!['chromium','firefox'].includes(name))throw Error('PACKAGE FRESH_OUTPUT chromium|firefox');
fs.mkdirSync(out);const rows=[],results=[];let context,server;
const wait=ms=>new Promise(r=>setTimeout(r,ms));
function check(v,m){if(!v)throw Error(m);results.push(m);console.log('PASS',m);}
(async()=>{
 server=spawn('python3',['tools/serve_browser.py',pkg,'--port',String(port)]);server.stderr.on('data',()=>{});
 await new Promise((r,j)=>{server.stdout.once('data',r);server.once('exit',c=>j(Error('server '+c)));});
 const type=name==='chromium'?chromium:firefox;
 async function open(){
  context=await type.launchPersistentContext(path.resolve(out,'profile'),{headless:true,viewport:{width:1280,height:1000},args:name==='chromium'?['--no-sandbox','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader','--disable-dev-shm-usage']:[]});
  const p=await context.newPage();p.latest=null;p.errors=[];
  p.on('console',m=>{const s=m.text();rows.push(s);if(s.includes('OFFLINE_PROBE '))try{p.latest=JSON.parse(s.split('OFFLINE_PROBE ')[1]);}catch(e){p.errors.push(e.message);}});
  p.on('pageerror',e=>{p.errors.push(e.stack);rows.push('PAGEERROR '+e.stack);});
  await p.goto('http://127.0.0.1:'+port,{timeout:120000});
  await p.waitForFunction(()=>['Play','Unavailable'].includes(document.querySelector('#play').textContent),null,{timeout:120000});
  check(await p.locator('#play').innerText()==='Play','preload and durable storage hydrate');
  await p.locator('#play').click();await p.locator('#canvas').click();await until(p,r=>r.audio?.ready,'actual audio clip ready');
  return p;
 }
 async function until(p,predicate,label,timeout=20000){const start=Date.now();while(Date.now()-start<timeout){if(p.errors.length)throw Error(p.errors.join('\n'));if(p.latest&&predicate(p.latest))return p.latest;await wait(40);}throw Error(label+': '+JSON.stringify(p.latest));}
 async function tap(p,k){await p.keyboard.down(k);await wait(80);await p.keyboard.up(k);}
 async function move(p,key,predicate){
  await p.keyboard.down(key);try{await until(p,predicate,'movement '+key);}finally{await p.keyboard.up(key);}
  let last=p.latest,stable=0;const start=Date.now();
  while(stable<3&&Date.now()-start<10000){await wait(40);const next=p.latest;if(next.sequence===last.sequence)continue;
   stable=Math.abs(next.position.x-last.position.x)<1e-6?stable+1:0;last=next;}
  check(stable===3,'released movement reaches an observed stationary endpoint');
 }
 async function save(p){await tap(p,'KeyK');return until(p,r=>r.status?.operation==='save'&&r.status.state==='completed','durable save');}
 async function bytes(p){return p.evaluate(()=>{let found;const root='/persistent/judas/games/'+JUDAS_GAME.saveIdentity;function walk(d){for(const n of Module.FS.readdir(d)){if(n==='.'||n==='..')continue;const q=d+'/'+n;if(Module.FS.isDir(Module.FS.stat(q).mode))walk(q);else if(n==='grove.save')found=Array.from(Module.FS.readFile(q));}}walk(root);return found;});}
 const digest=b=>crypto.createHash('sha256').update(Buffer.from(b)).digest('hex');
 let p=await open();check(p.latest.state.health===3&&p.latest.gateValid,'fresh real game and closed gate');
 // Tap native miniaudio output, never synthesize a test tone in JS.
 await p.evaluate(()=>{const d=window.miniaudio?.devices.find(d=>d?.webaudio);if(!d)throw Error('no real audio device');const a=d.webaudio.createAnalyser(),g=d.webaudio.createGain();g.gain.value=0;d.scriptNode.connect(a);a.connect(g);g.connect(d.webaudio.destination);window.audioProbe={a,context:d.webaudio};});
 await p.locator('#canvas').click();await wait(500);
 const sound=await p.evaluate(()=>{let d=new Float32Array(audioProbe.a.fftSize);audioProbe.a.getFloatTimeDomainData(d);return {state:audioProbe.context.state,rms:Math.sqrt(d.reduce((a,v)=>a+v*v,0)/d.length)};});
 check(sound.state==='running'&&sound.rms>.02&&sound.rms<.12,'real engine decoded PCM reaches Web Audio output');
 await tap(p,'Escape');await wait(350);const paused=p.latest;
 await p.keyboard.down('KeyD');await wait(400);await p.keyboard.up('KeyD');await wait(200);
 check(p.latest.state.time===paused.state.time&&Math.abs(p.latest.position.x-paused.position.x)<1e-6,'authored pause menu prevents simulation and movement');
 await tap(p,'Escape');await until(p,r=>r.state.time>paused.state.time,'resume');
 await tap(p,'KeyE');await until(p,r=>r.state.pulses>=1,'animation event');check(!p.latest.state.charged&&p.latest.gateValid,'animation event alone cannot open gate');
 await move(p,'KeyD',r=>r.position.x>3.8);check(p.latest.state.cells.length===2,'actual trigger pickups collect two cells');
 const saved=await save(p),savedPose=saved.position;const savedBytes=await bytes(p);check(savedBytes?.length>100,'real binary progress saved');const hash=digest(savedBytes);
 await p.screenshot({path:path.join(out,'partial.png')});check(!p.errors.length,'no runtime exceptions before cold restart');
 await context.close();context=null;p=await open();check(digest(await bytes(p))===hash,'new browser process hydrates exact saved bytes');
 await tap(p,'KeyL');const restored=await until(p,r=>r.status?.operation==='load'&&r.status.state==='completed','cold load');
 check(restored.state.cells.length===2&&Math.abs(restored.position.x-savedPose.x)<.01&&restored.gateValid,'cold load restores actual physical endpoint, inventory and closed gate');
 await move(p,'KeyD',r=>r.state.cells.length===3);
 if(p.latest.state.nearBeacon)await move(p,'KeyA',r=>r.position.x<5.6);
 check(p.latest.state.cells.length===3,'third real pickup after cold load');
 const pulse=p.latest.state.pulses;await tap(p,'KeyE');await until(p,r=>r.state.pulses>pulse,'second animation marker');check(!p.latest.state.charged&&p.latest.gateValid,'all resources without proximity cannot open gate');
 await move(p,'KeyD',r=>r.position.x>7.8);await tap(p,'KeyE');await until(p,r=>r.state.charged&&!r.gateValid,'animation-timed physical gate removal');
 await move(p,'KeyD',r=>r.state.won);check(p.latest.position.x>11,'actual player crosses removed gate and reaches victory');
 await save(p);await p.screenshot({path:path.join(out,'victory.png')});
 await tap(p,'KeyR');await until(p,r=>!r.state.won&&r.state.cells.length===0,'scene restart');await tap(p,'KeyL');
 await until(p,r=>r.state.won&&!r.gateValid&&r.status?.state==='completed','completed game reload');
 check(p.latest.state.cells.length===3&&p.latest.position.x>11,'completed save restores progress and actual removed gate');
 await tap(p,'KeyR');await until(p,r=>!r.state.won&&r.state.health===3,'loss reset');
 await move(p,'KeyA',r=>r.position.x< -2.7);await until(p,r=>r.state.lost,'real hazard loss');check(p.latest.state.health===0,'damage and defeat are actual gameplay');
 await tap(p,'KeyR');await until(p,r=>!r.state.lost&&r.state.health===3,'retry');
 check(!p.errors.length,'complete win/loss/reload loop without runtime exceptions');
 const version=context.browser().version();
 await tap(p,'Escape');await tap(p,'ArrowDown');await tap(p,'ArrowDown');await tap(p,'Enter');
 await p.waitForFunction(()=>document.querySelector('#status').textContent.includes('Game exited (0)'),null,{timeout:20000});
 check(await p.evaluate(()=>!window.miniaudio),'ordinary UI Quit destroys the actual audio device before runtime shutdown');
 fs.writeFileSync(path.join(out,'RESULTS.json'),JSON.stringify({passed:true,browser:name,version,checks:results,sound,savedPose,restoredPose:restored.position,savedSha256:hash,savedBytes:savedBytes.length},null,2));
})().catch(e=>{console.error(e.stack);process.exitCode=1;}).finally(async()=>{fs.writeFileSync(path.join(out,'console.log'),rows.join('\n'));if(context)await context.close();if(server)server.kill('SIGTERM');});
