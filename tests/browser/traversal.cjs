// Public observer fixture: copy Traversal and emit position/velocity/status from controls.js.
// PLAYWRIGHT_MODULE=... node this.cjs PACKAGE FRESH_OUTPUT chromium|firefox
const {chromium,firefox}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('fs'),path=require('path'),{spawn}=require('child_process');
const [pkg,out,name='chromium']=process.argv.slice(2);fs.mkdirSync(out);
const rows=[],checks=[];let browser,server;const delay=ms=>new Promise(r=>setTimeout(r,ms));
const check=(v,m)=>{if(!v)throw Error(m);checks.push(m);console.log('PASS',m);};
(async()=>{
 server=spawn('python3',['tools/serve_browser.py',pkg,'--port','8797']);server.stderr.on('data',()=>{});
 await new Promise((r,j)=>{server.stdout.once('data',r);server.once('exit',c=>j(Error('server '+c)));});
 browser=await (name==='firefox'?firefox:chromium).launch({headless:true,args:name==='chromium'?['--no-sandbox','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader','--disable-dev-shm-usage']:[]});
 const p=await browser.newPage({viewport:{width:1280,height:1000}});let latest,errors=[];
 p.on('console',m=>{rows.push(m.text());if(m.text().includes('BROWSER_PROBE '))latest=JSON.parse(m.text().split('BROWSER_PROBE ')[1]);});p.on('pageerror',e=>errors.push(e.stack));
 async function until(fn,label){const start=Date.now();while(Date.now()-start<20000){if(errors.length)throw Error(errors.join('\n'));if(latest&&fn(latest))return latest;await delay(30);}throw Error(label+': '+JSON.stringify(latest));}
 async function tap(k){await p.keyboard.down(k);await delay(80);await p.keyboard.up(k);}
 const at=(x,y)=>r=>Math.abs(r.position.x-x)<.01&&Math.abs(r.position.y-y)<.01;
 await p.goto('http://127.0.0.1:8797',{timeout:120000});await p.waitForFunction(()=>document.querySelector('#play').textContent==='Play',null,{timeout:120000});await p.locator('#play').click();await p.locator('#canvas').click();await until(at(0,.1),'initial ground');
 await p.keyboard.down('KeyD');await until(r=>r.position.x>2.3,'move underneath');await p.keyboard.up('KeyD');await until(r=>Math.abs(r.velocity.x)<.001,'stop below platform');
 check(latest.position.x>1.9&&latest.position.x<4&&Math.abs(latest.position.y-.1)<.01,'walk underneath actual one-way collider');
 await tap('Space');await until(r=>r.position.y>1.5&&r.velocity.y>0,'jump through from below');
 await until(r=>Math.abs(r.position.y-1.3)<.02&&Math.abs(r.velocity.y)<.05,'land on top');
 check(true,'jump through underside and land on real one-way top');await p.screenshot({path:path.join(out,'landed.png')});
 await tap('Digit2');await until(at(0,2),'grid scene switch');await tap('KeyD');await until(at(1,2),'free neighbour');await tap('KeyD');await delay(1500);
 check(at(1,2)(latest),'actual blocked tile rejects full-cell movement from west');
 await tap('KeyS');await until(at(1,1),'step below wall');await tap('KeyD');await until(at(2,1),'free cell below wall');await tap('KeyW');await delay(1500);
 check(at(2,1)(latest),'actual blocked tile rejects full-cell movement from south');
 await tap('KeyA');await until(at(1,1),'step around wall');await tap('KeyW');await until(at(1,2),'free cell beside wall');check(true,'free neighbour remains traversable');
 await tap('KeyK');await until(r=>r.status?.operation==='save'&&r.status.state==='completed','grid durable save');check(await p.evaluate(()=>Module.judasSaveFlushes>0),'grid save reaches IndexedDB publication');
 await tap('Digit1');await until(at(0,.1),'return to platform scene');await tap('KeyL');await until(r=>r.requestTwo?.operation==='load'&&r.requestTwo.state==='completed'&&at(1,2)(r),'restore saved grid scene');
 check(true,'save/load restores actual grid pose across scene transition');await p.screenshot({path:path.join(out,'grid-restored.png')});check(!errors.length,'no browser runtime errors');
 fs.writeFileSync(path.join(out,'RESULTS.json'),JSON.stringify({passed:true,browser:name,version:browser.version(),checks,restored:latest},null,2));
})().catch(e=>{console.error(e.stack);process.exitCode=1;}).finally(async()=>{fs.writeFileSync(path.join(out,'console.log'),rows.join('\n'));if(browser)await browser.close();if(server)server.kill('SIGTERM');});
