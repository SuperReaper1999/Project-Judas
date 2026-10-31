const {chromium}=require('../browser-tests/node_modules/playwright');
const fs=require('fs'),{spawn}=require('child_process');
(async()=>{
 const server=spawn('python3',['tools/serve_browser.py','.cache/browser-proof/traversal-licensed','--port','8792']);
 const serverOutput=[];server.stderr.on('data',d=>serverOutput.push(String(d)));
 await new Promise((resolve,reject)=>{server.stdout.once('data',resolve);server.once('exit',c=>reject(Error('server exited '+c)));});
 const browser=await chromium.launch({headless:true,args:['--no-sandbox','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader','--disable-dev-shm-usage']});
 const context=await browser.newContext({viewport:{width:1280,height:1000}});
 const page=await context.newPage(),messages=[];
 page.on('console',msg=>{messages.push(msg.type()+': '+msg.text());fs.writeFileSync('.cache/browser-sdk/boot-final-console.log',messages.join('\n'));});
 page.on('pageerror',e=>messages.push('PAGEERROR: '+e.stack));
 try{
  await page.goto('http://127.0.0.1:8792',{timeout:120000});
  await page.waitForFunction(()=>['Play','Unavailable'].includes(document.querySelector('#play').textContent),null,{timeout:120000});
  if(await page.locator('#play').innerText()!=='Play')throw Error(await page.locator('#status').innerText());
  console.log('READY',await page.locator('#status').innerText());
  await page.locator('#play').click();
  for(let i=0;i<100&&!messages.some(x=>x.includes('Scene:'));i++)await page.waitForTimeout(100);
  console.log(messages.slice(-20).join('\n'));
  if(!messages.some(x=>x.includes('Scene:')))throw Error('Native game did not reach scene startup');
  await page.waitForTimeout(1500);
  await page.screenshot({path:'.cache/browser-sdk/first-game-final.png',fullPage:true});
  await page.locator('#canvas').click();await page.keyboard.press('Digit2');await page.waitForTimeout(1000);
  await page.keyboard.down('KeyD');await page.waitForTimeout(70);await page.keyboard.up('KeyD');await page.waitForTimeout(700);
  await page.screenshot({path:'.cache/browser-sdk/grid-final.png',fullPage:true});
  await page.keyboard.press('F6');
  await page.waitForFunction(()=>Module.judasSaveFlushes>0,null,{timeout:25000});
  console.log('DURABLE_FLUSHES',await page.evaluate(()=>Module.judasSaveFlushes));
  await page.screenshot({path:'.cache/browser-sdk/save-final.png',fullPage:true});
  if(messages.some(x=>x.startsWith('PAGEERROR:')||x.includes('Game exited (1)')))throw Error('Browser runtime faulted');
  console.log('PASS actual Chromium game/input/IndexedDB flush');
 }catch(e){console.error(e.stack);console.log('STATUS',await page.locator('#status').innerText().catch(()=>''));process.exitCode=1;}
 finally{fs.writeFileSync('.cache/browser-sdk/boot-final-console.log',messages.join('\n'));fs.writeFileSync('.cache/browser-sdk/server-final.log',serverOutput.join(''));await browser.close();server.kill('SIGTERM');}
})();
