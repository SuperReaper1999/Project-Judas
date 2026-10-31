const {firefox}=require('../browser-tests/node_modules/playwright');
const {spawn,execFileSync}=require('child_process');const fs=require('fs');
const wait=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{let browser,server;try{
server=spawn('python3',['.cache/browser-player/final-player-web/serve.py','.cache/browser-player/final-player-web','--port','8801']);
await wait(700);browser=await firefox.launch({headless:false});const page=await browser.newPage({viewport:{width:1280,height:900}});
let counts={};page.on('console',m=>{counts[m.type()]=(counts[m.type()]||0)+1;if(m.type()==='error'&&counts.error<6)console.log('ERROR',m.text().slice(0,1000));});page.on('pageerror',e=>console.log('PAGEERROR',String(e).slice(0,1000)));
await page.goto('http://127.0.0.1:8801');await page.waitForFunction(()=>document.querySelector('#play').textContent==='Play',null,{timeout:90000});
console.log('READY');await page.locator('#play').click();await page.locator('#canvas').click({position:{x:150,y:150}});
for(let i=0;i<18;i++){await page.keyboard.down('KeyW');for(let j=0;j<10;j++){await page.mouse.move(200+(j%2)*200,250);await wait(100);}await page.keyboard.up('KeyW');await wait(4000);console.log('SAMPLE',i,JSON.stringify(counts),execFileSync('ps',['-eo','pid,ppid,rss,comm'],{encoding:'utf8'}).split('\n').filter(s=>/firefox|Web Content|Isolated|RDD|Socket|Privileged/.test(s)).join(';'));}
console.log('DONE');}finally{if(browser)await browser.close();if(server)server.kill();}})().catch(e=>{console.error(e);process.exitCode=1});
