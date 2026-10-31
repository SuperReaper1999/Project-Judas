// Real packaged WASM: matched cold HTTP-cache and revalidated warm loads.
// PLAYWRIGHT_MODULE=... node delivery.cjs COMPRESSED IDENTITY FRESH_OUTPUT [chromium|firefox]
const {chromium,firefox}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('fs'),path=require('path'),{spawn}=require('child_process');
const [compressed,identity,directory,engine='chromium']=process.argv.slice(2);
fs.mkdirSync(directory);const out=path.resolve(directory),runs=[],servers=[];
const wait=ms=>new Promise(r=>setTimeout(r,ms));
const options=engine==='chromium'?{channel:'chromium',args:['--no-sandbox','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader','--disable-dev-shm-usage']}:{};
let browser;
function assert(value,message){if(!value)throw Error(message);}
async function serve(pkg,label){
 const log=fs.createWriteStream(path.join(out,label+'-http.log'));
 const child=spawn('python3',[path.resolve(pkg,'serve.py'),path.resolve(pkg),'--port','0']);servers.push(child);
 child.stderr.pipe(log);
 return await new Promise((resolve,reject)=>{child.stdout.once('data',b=>resolve(String(b).trim().split(' ').pop()));child.once('exit',c=>reject(Error('server exited '+c)));});
}
(async()=>{
 const urls={compressed:await serve(compressed,'compressed'),identity:await serve(identity,'identity')};
 const variants=engine==='chromium'?['identity','compressed','compressed','identity','identity','compressed']:['compressed'];
 for(let i=0;i<variants.length;i++){
  const variant=variants[i];
  // Real user profiles permit disk caching; an incognito memory-only cache can
  // discard these large resources and would not measure ordinary repeat visits.
  const context=await ({chromium,firefox}[engine]).launchPersistentContext(path.join(out,'profile-'+i),{headless:true,...options,viewport:{width:1024,height:768}});
  browser=context.browser();const page=await context.newPage();
  const errors=[],network=[],finished=[],extra=[];let latest;
  page.on('pageerror',e=>errors.push(e.message));
  page.on('console',m=>{if(m.text().includes('PLAYER_PROBE '))try{latest=JSON.parse(m.text().split('PLAYER_PROBE ')[1]);}catch(e){errors.push(e.message);}});
  let cdp;
  if(engine==='chromium'){
   cdp=await context.newCDPSession(page);await cdp.send('Network.enable');
   await cdp.send('Network.emulateNetworkConditions',{offline:false,latency:40,downloadThroughput:4*1024*1024,uploadThroughput:1024*1024});
   cdp.on('Network.loadingFinished',e=>finished.push(e));
   cdp.on('Network.responseReceivedExtraInfo',e=>extra.push({id:e.requestId,status:e.statusCode}));
  }
  page.on('response',r=>network.push({url:r.url(),status:r.status(),headers:r.headers()}));
  const row={sample:i,variant,browser:browser.version(),engine,loads:[]};
  for(const warm of [false,true]){
   network.length=finished.length=extra.length=0;const start=Date.now();
   if(warm)await page.reload({waitUntil:'domcontentloaded',timeout:120000});
   else await page.goto(urls[variant],{waitUntil:'domcontentloaded',timeout:120000});
   await page.waitForFunction(()=>['Play','Unavailable'].includes(document.querySelector('#play').textContent),null,{timeout:120000});
   assert(await page.locator('#play').innerText()==='Play','runtime did not become ready: '+await page.locator('#status').innerText());
   assert(await page.locator('#loading').isHidden(),'loading indicator remains after ready');
   const millis=Date.now()-start;
   const resources=await page.evaluate(()=>performance.getEntriesByType('resource').map(r=>({name:r.name,transferSize:r.transferSize,encodedBodySize:r.encodedBodySize,decodedBodySize:r.decodedBodySize,duration:r.duration})));
   const payload=network.find(r=>r.url.endsWith('/judas_browser.wasm'));
   assert(payload,'actual WASM response not observed');
   assert((payload.headers['content-encoding']==='gzip')===(variant==='compressed'),'incorrect transfer encoding');
   row.loads.push({warm,readyMs:millis,network:[...network],finished:[...finished],httpStatuses:[...extra],resources});
   console.log(engine,variant,warm?'warm':'cold',millis+' ms');
  }
  fs.writeFileSync(path.join(out,'sample-'+i+'-loading.json'),JSON.stringify(row,null,2));
  await page.locator('#play').click();
  const deadline=Date.now()+30000;
  while((!latest||!latest.supported)&&Date.now()<deadline)await wait(50);
  assert(latest?.supported,'real character did not land');
  const start={...latest.position};await page.evaluate(()=>document.exitPointerLock?.());await wait(150);await page.locator('#canvas').click({position:{x:100,y:100}});await page.keyboard.down('KeyD');await wait(600);await page.keyboard.up('KeyD');await wait(200);
  const distance=Math.hypot(latest.position.x-start.x,latest.position.z-start.z);
  assert(distance>.1,'actual keyboard movement did not advance player');
  await page.keyboard.press('KeyV');await wait(200);
  assert(latest.state.thirdPerson,'camera toggle did not reach gameplay');
  assert(errors.length===0,errors.join('\n'));
  row.gameplay={distance,latest,errors};
  if(i===0||i===variants.length-1)await page.screenshot({path:path.join(out,variant+'-'+engine+'.png')});
  runs.push(row);fs.writeFileSync(path.join(out,'RESULTS.json'),JSON.stringify({engine,network:engine==='chromium'?{latencyMs:40,downloadBytesPerSecond:4*1024*1024}:null,runs},null,2));
  await context.close();await browser.close();browser=null;
 }
 console.log('PASS real WASM cold/warm delivery and gameplay',engine,runs.length+' samples');
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();for(const server of servers)server.kill();});
