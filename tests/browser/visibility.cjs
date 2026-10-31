// Attach to an owned normal browser without Playwright's forced visibility/focus.
// PLAYWRIGHT_MODULE=... node visibility.cjs PACKAGE FRESH_OUTPUT
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('fs'),path=require('path'),{spawn}=require('child_process');
const [pkg,out]=process.argv.slice(2);fs.mkdirSync(out);const rows=[],checks=[];
const wait=ms=>new Promise(r=>setTimeout(r,ms));let server,processChrome,browser;
function check(v,m){if(!v)throw Error(m);checks.push(m);console.log('PASS',m);}
(async()=>{
 server=spawn('python3',[path.resolve(pkg,'serve.py'),path.resolve(pkg),'--port','8799'],{cwd:path.resolve(out)});
 server.stderr.on('data',b=>rows.push(String(b)));
 await new Promise((r,j)=>{server.stdout.once('data',r);server.once('exit',c=>j(Error('server '+c)));});
 processChrome=spawn(chromium.executablePath(),['--remote-debugging-port=0','--remote-debugging-address=127.0.0.1',
  '--user-data-dir='+path.resolve(out,'profile'),'--no-sandbox','--no-first-run','--no-default-browser-check',
  '--window-size=1280,960','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader','--disable-dev-shm-usage','about:blank']);
 const endpoint=await new Promise((resolve,reject)=>{let s='';const timer=setTimeout(()=>reject(Error('browser startup')),15000);
  processChrome.stderr.on('data',b=>{s+=b;const m=s.match(/DevTools listening on (ws:\/\/[^\s]+)/);if(m){clearTimeout(timer);resolve(m[1]);}});processChrome.once('exit',c=>reject(Error('browser exited '+c)));});
 browser=await chromium.connectOverCDP(endpoint,{noDefaults:true});const context=browser.contexts()[0];
 const p=context.pages()[0];let latest;const errors=[];
 p.on('console',m=>{const s=m.text();rows.push(s);for(const prefix of ['PLAYER_PROBE ','OFFLINE_PROBE '])if(s.includes(prefix))latest=JSON.parse(s.split(prefix)[1]);});
 p.on('pageerror',e=>errors.push(e.stack));
 await p.goto('http://127.0.0.1:8799');await p.locator('#play').getByText('Play',{exact:true}).waitFor({timeout:120000});
 await p.locator('#play').click();await p.locator('#canvas').click({position:{x:100,y:100}});
 async function until(predicate,label){const start=Date.now();while(Date.now()-start<15000){if(errors.length)throw Error(errors.join('\n'));if(latest&&predicate(latest))return latest;await wait(40);}throw Error(label+': '+JSON.stringify(latest));}
 await until(r=>r.state.time>0,'actual gameplay');
 const audio=()=>p.evaluate(()=>window.miniaudio?.devices.filter(d=>d?.webaudio).map(d=>d.webaudio.state)||[]);
 await wait(300);const beforeAudio=await audio();rows.push('AUDIO_BEFORE '+JSON.stringify(beforeAudio));
 const movingStart=latest.position.x;await p.keyboard.down('KeyD');await until(r=>r.position.x>movingStart+.1,'real movement before tab switch');
 const other=await context.newPage();await other.bringToFront();await other.keyboard.up('KeyD');
 await p.waitForFunction(()=>document.hidden,null,{timeout:5000,polling:50});check(true,'normal browser genuinely hides the game tab');
 await p.waitForFunction(()=>Module.judasSurface?.active===false,null,{timeout:5000,polling:50});check(true,'hidden state reaches native application lifecycle');
 await wait(400);const stopped=latest;const epochHidden=await p.evaluate(()=>Module.judasVisibilityEpoch);
 await wait(800);check(latest.sequence===stopped.sequence,'game frames and fixed simulation stop while hidden');
 const hiddenAudio=await audio();
 if(beforeAudio.length){check(beforeAudio.every(s=>s==='running'),'real audio device was running before hide');check(hiddenAudio.length===beforeAudio.length&&hiddenAudio.every(s=>s==='suspended'),'native audio device suspends');}
 await p.bringToFront();const resumed=await until(r=>r.sequence>stopped.sequence,'foreground resumes');
 check(resumed.state.time-stopped.state.time<.5,'foreground clock avoids hidden-time catch-up');
 // The ordinary controller preserves momentum and brakes with authored
 // acceleration. Check the late stationary endpoint, not an artificial stop.
 const settled=await until(r=>r.sequence>=resumed.sequence+20,'returned input settles');
 const later=await until(r=>r.sequence>=settled.sequence+20,'released key remains settled');
 check(Math.abs(later.position.x-settled.position.x)<.01,'key released in another tab cannot stick on return');
 const epochVisible=await p.evaluate(()=>Module.judasVisibilityEpoch);check(epochVisible>epochHidden&&epochHidden>0,'UI visibility epochs retain both transitions');
 if(beforeAudio.length){await p.waitForFunction(()=>window.miniaudio.devices.filter(d=>d?.webaudio).every(d=>d.webaudio.state==='running'),null,{timeout:5000,polling:50});check(true,'native audio resumes');}
 check(!errors.length,'no runtime exceptions');
 fs.writeFileSync(path.join(out,'RESULTS.json'),JSON.stringify({passed:true,browser:browser.version(),checks,beforeAudio,hiddenAudio,epochHidden,epochVisible,stopped,resumed,settled,later},null,2));
})().catch(e=>{console.error(e.stack);process.exitCode=1;}).finally(async()=>{fs.writeFileSync(path.join(out,'console.log'),rows.join('\n'));if(browser)await browser.close();if(processChrome)processChrome.kill('SIGTERM');if(server)server.kill('SIGTERM');});
