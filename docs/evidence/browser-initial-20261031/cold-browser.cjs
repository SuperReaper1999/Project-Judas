const {chromium}=require('../browser-tests/node_modules/playwright');
const fs=require('fs'),crypto=require('crypto'),{spawn}=require('child_process');
const packageDir=process.argv[2],base='.cache/browser-sdk/cold',profile=base+'-profile-'+Date.now(),port=8793;
const rows=[];const assert=(v,m)=>{if(!v)throw Error(m);console.log('PASS',m);};
const digest=b=>crypto.createHash('sha256').update(Buffer.from(b)).digest('hex');
const delay=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
 const server=spawn('python3',['tools/serve_browser.py',packageDir,'--port',String(port)]);
 server.stderr.on('data',()=>{});await new Promise((r,j)=>{server.stdout.once('data',r);server.once('exit',c=>j(Error('Server exit '+c)));});
 let context;
 async function open(){
  context=await chromium.launchPersistentContext(profile,{headless:true,viewport:{width:1280,height:1000},args:['--no-sandbox','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader','--disable-dev-shm-usage']});
  const page=await context.newPage();page.probes=[];page.errors=[];
  page.on('console',m=>{const text=m.text();rows.push(text);if(text.includes('BROWSER_PROBE ')){try{page.probes.push(JSON.parse(text.split('BROWSER_PROBE ')[1]));}catch(e){page.errors.push(e.message);}}});
  page.on('pageerror',e=>{rows.push('PAGEERROR '+e.stack);page.errors.push(e.message);});
  await page.goto('http://127.0.0.1:'+port,{timeout:120000});
  await page.waitForFunction(()=>['Play','Unavailable'].includes(document.querySelector('#play').textContent),null,{timeout:120000});
  assert(await page.locator('#play').innerText()==='Play','game preloads and hydrates persistent storage');return page;
 }
 async function play(p){await p.locator('#play').click();await p.locator('#canvas').click();for(let i=0;i<100&&!p.probes.length;i++)await delay(100);assert(p.probes.length>0,'actual script reports native player pose');}
 async function at(p,x){for(let i=0;i<100;i++){const r=p.probes.at(-1);if(r&&Math.abs(r.position.x-x)<1e-5)return r;await delay(100);}throw Error('Position never reached '+x+': '+JSON.stringify(p.probes.at(-1)));}
 async function tap(p,key){await p.keyboard.down(key);await delay(60);await p.keyboard.up(key);}
 async function slot(p){return p.evaluate(()=>{const root='/persistent/judas/games/'+JUDAS_GAME.saveIdentity;let found;function walk(d){for(const n of Module.FS.readdir(d)){if(n==='.'||n==='..')continue;const p=d+'/'+n,st=Module.FS.stat(p);if(Module.FS.isDir(st.mode))walk(p);else if(n==='traversal.save')found=Array.from(Module.FS.readFile(p));}}walk(root);return found;});}
 try{
  let p=await open();await play(p);await at(p,0);await tap(p,'KeyA');await at(p,-1);
  await p.screenshot({path:base+'-moved.png',fullPage:true});
  for(let n=1;n<=2;n++){await tap(p,'F6');await p.waitForFunction(n=>Module.judasSaveFlushes>=n,n,{timeout:25000});for(let i=0;i<100&&p.probes.at(-1).status?.state!=='completed';i++)await delay(100);assert(p.probes.at(-1).status?.state==='completed','save '+n+' reports actual completed durable publication');}
  const bytes=await slot(p);assert(bytes?.length>100,'real binary save exists');const hash=digest(bytes);
  const second=await context.newPage();await second.goto('http://127.0.0.1:'+port);await second.waitForFunction(()=>document.querySelector('#play').textContent==='Unavailable',null,{timeout:120000});assert((await second.locator('#status').innerText()).includes('another tab'),'same-game competing tab fails closed');await second.close();
  assert(!p.errors.length,'no browser runtime exceptions');await context.close();context=null;
  p=await open();assert(digest(await slot(p))===hash,'fresh Chromium process restores exact saved bytes from IndexedDB');await play(p);await at(p,0);await tap(p,'KeyA');await at(p,-1);await tap(p,'KeyA');await at(p,-2);await tap(p,'F7');await at(p,-1);for(let i=0;i<100&&p.probes.at(-1).requestOne?.operation!=='load';i++)await delay(100);
  assert(!p.errors.length,'cold load has no browser runtime exceptions');
  assert(p.probes.at(-1).requestOne?.state==='completed'&&p.probes.at(-1).requestOne?.operation==='load','fresh-process load service reports completed and actual endpoint matches');
  await p.screenshot({path:base+'-restored.png',fullPage:true});
  fs.writeFileSync(base+'-RESULTS.json',JSON.stringify({passed:true,savedBytes:bytes.length,savedSha256:hash,restored:p.probes.at(-1),renderer:'Chromium ANGLE SwiftShader; software functional validation only'},null,2));
 }catch(e){console.error(e.stack);process.exitCode=1;}
 finally{fs.writeFileSync(base+'-console.log',rows.join('\n'));if(context)await context.close();server.kill('SIGTERM');}
})();
