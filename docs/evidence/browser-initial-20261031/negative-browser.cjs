const {chromium}=require('../browser-tests/node_modules/playwright');
const {spawn}=require('child_process'),fs=require('fs');
(async()=>{let browser;const servers=[];const results=[];try{
 async function serve(args){const s=spawn('python3',['-u',...args]);servers.push(s);s.stderr.on('data',()=>{});await new Promise((r,j)=>{s.stdout.once('data',r);s.once('exit',c=>j(Error('server '+c)));});}
 await serve(['tools/serve_browser.py',process.argv[2],'--port','8794']);
 await serve(['-m','http.server','8795','--bind','127.0.0.1','--directory',process.argv[2]]);
 browser=await chromium.launch({headless:true,args:['--no-sandbox','--disable-dev-shm-usage','--enable-unsafe-swiftshader']});
 for(const [name,port,blocked] of [['storage-denied',8794,true],['missing-isolation',8795,false]]){
  const context=await browser.newContext();if(blocked)await context.addInitScript(()=>Object.defineProperty(globalThis,'indexedDB',{get(){throw Error('deliberate IndexedDB denial');}}));
  const page=await context.newPage();page.on('pageerror',()=>{});await page.goto('http://127.0.0.1:'+port,{timeout:120000});
  await page.waitForFunction(()=>document.querySelector('#play').textContent==='Unavailable',null,{timeout:120000});
  const text=await page.locator('#status').innerText();if(!await page.locator('#play').isDisabled())throw Error('Game started despite '+name);
  results.push({name,passed:true,message:text});console.log('PASS',name,text);await context.close();
 }
 fs.writeFileSync('.cache/browser-sdk/negative-RESULTS.json',JSON.stringify(results,null,2));
}catch(e){console.error(e.stack);process.exitCode=1;}finally{if(browser)await browser.close();for(const s of servers)s.kill('SIGTERM');}})();
