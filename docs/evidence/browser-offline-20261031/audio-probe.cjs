const {chromium,firefox}=require('../browser-tests/node_modules/playwright');
const fs=require('fs'),{spawn}=require('child_process');
(async()=>{let browser;const server=spawn('python3',['tools/serve_browser.py','.cache/browser-stage2','--port','8788']);server.stderr.on('data',()=>{});await new Promise((r,j)=>{server.stdout.once('data',r);server.once('exit',c=>j(Error('server '+c)));});const results=[];
try{for(const [name,type] of [['chromium',chromium],['firefox',firefox]]){
 browser=await type.launch({headless:true,args:name==='chromium'?['--no-sandbox','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader','--disable-dev-shm-usage']:[]});
 for(const variant of ['original','proxy']){
  const page=await browser.newPage(),logs=[];page.on('console',m=>logs.push(m.text()));page.on('pageerror',e=>logs.push('PAGEERROR '+e.stack));
  await page.goto('http://127.0.0.1:8788/audio-'+variant+'.html');await page.locator('#start').click();
  for(let i=0;i<100&&!logs.some(x=>x.includes('AUDIO_INIT'));i++)await page.waitForTimeout(100);
  console.log('START',name,variant,JSON.stringify(logs),await page.evaluate(()=>({devices:window.miniaudio?.devices?.map(d=>d?Object.keys(d):null),keys:Object.keys(window).filter(k=>k.includes('miniaudio'))})));fs.writeFileSync('.cache/browser-stage2/'+name+'-'+variant+'-startup.json',JSON.stringify(logs,null,2));let rms=0,state='absent';
  if(variant==='proxy'){
   await page.locator('#start').evaluate(el=>{el.disabled=true;}); // do not create a second device
   await page.evaluate(()=>{let b=document.createElement('button');b.id='unlock';b.textContent='Enable sound';document.body.appendChild(b);});await page.locator('#unlock').click();
   await page.evaluate(()=>{const d=window.miniaudio?.devices.find(d=>d?.webaudio);if(!d)throw Error('no backend device');const a=d.webaudio.createAnalyser(),g=d.webaudio.createGain();g.gain.value=0;d.scriptNode.connect(a);a.connect(g);g.connect(d.webaudio.destination);window.probe={context:d.webaudio,a};});
   await page.waitForTimeout(700);
   const out=await page.evaluate(()=>{const data=new Float32Array(probe.a.fftSize);probe.a.getFloatTimeDomainData(data);return {state:probe.context.state,rms:Math.sqrt(data.reduce((a,v)=>a+v*v,0)/data.length)};});rms=out.rms;state=out.state;
  }
  const expected=variant==='original'?logs.some(x=>/AUDIO_INIT -/.test(x)):logs.some(x=>x==='AUDIO_PLAY 0')&&state==='running'&&rms>.03&&rms<.12;
  results.push({browser:name,version:browser.version(),variant,expected,rms,state,logs});console.log(JSON.stringify(results.at(-1)));if(!expected)throw Error(name+' '+variant+' failed');await page.close();
 }
 await browser.close();browser=null;
}fs.writeFileSync('.cache/browser-stage2/audio-probe-results.json',JSON.stringify(results,null,2));}
catch(e){console.error(e.stack);fs.writeFileSync('.cache/browser-stage2/audio-probe-partial.json',JSON.stringify(results,null,2));process.exitCode=1;}
finally{if(browser)await browser.close();server.kill('SIGTERM');}})();
