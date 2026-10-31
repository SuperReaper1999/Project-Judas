// Installed-Firefox hardware endurance. Run only inside the bounded unit below.
const fs=require('fs'),path=require('path'),{spawn}=require('child_process');
const root=path.resolve('.cache/browser-shadow'),out=path.join(root,'endurance');fs.mkdirSync(out,{recursive:true});
const cg=fs.readFileSync('/proc/self/cgroup','utf8').trim().split('::')[1],group='/sys/fs/cgroup'+cg;
if(!cg.includes('judas-browser-endurance'))throw Error('Use the dedicated memory-limited service');
const sleep=ms=>new Promise(r=>setTimeout(r,ms));const started=Date.now();let sid,driver,server,phase='startup';
const memory=[],snapshots=[],cycles=[],commands=[],errors=[];
function readMemory(){const stat=Object.fromEntries(fs.readFileSync(group+'/memory.stat','utf8').trim().split('\n').map(l=>{const [k,v]=l.split(' ');return[k,Number(v)]}));return {seconds:(Date.now()-started)/1000,phase,bytes:Number(fs.readFileSync(group+'/memory.current','utf8')),anon:stat.anon,shmem:stat.shmem};}
const timer=setInterval(()=>{const m=readMemory();memory.push(m);if(memory.length%30===0)console.log('MEMORY',JSON.stringify(m));if(m.bytes>2200*1024*1024){console.error('EARLY_STOP memory limit');save(false);process.exit(2)}},1000);
function save(passed){fs.writeFileSync(path.join(out,'RESULTS.json'),JSON.stringify({passed,started:new Date(started).toISOString(),memory,snapshots,cycles,commands,errors,gates:{activeSeconds:600,cycles:3,cycleActiveSeconds:30,plateauGrowthMiB:128,afterNavigationGrowthMiB:64,afterBrowserExitMiB:256}},null,2));}
async function call(route,data,method='POST'){const r=await fetch('http://127.0.0.1:4446'+route,{method,headers:{'content-type':'application/json'},...(data===undefined?{}:{body:JSON.stringify(data)}),signal:AbortSignal.timeout(90000)});const j=await r.json();if(j.value?.error)throw Error(JSON.stringify(j));return j.value;}
const run=script=>call('/session/'+sid+'/execute/sync',{script,args:[]});
async function click(selector){const e=await call('/session/'+sid+'/element',{using:'css selector',value:selector});await call('/session/'+sid+'/element/'+e['element-6066-11e4-a52e-4f735466cecf']+'/click',{});}
async function open(){const session=await call('/session',{capabilities:{alwaysMatch:{browserName:'firefox','moz:firefoxOptions':{binary:'/snap/firefox/current/usr/lib/firefox/firefox',args:['--no-remote'],prefs:{'browser.shell.checkDefaultBrowser':false}}}}});sid=session.sessionId;console.log('BROWSER',session.capabilities.browserVersion);await call('/session/'+sid+'/window/rect',{width:1600,height:1000});await call('/session/'+sid+'/url',{url:'http://127.0.0.1:8804'});
 for(let i=0;i<180;i++){if(await run("return document.querySelector('#play').textContent==='Play'"))break;if(i===179)throw Error('preload timeout');await sleep(500);}
 await run("window.memProbe={count:0,last:null};const old=console.log;console.log=(...a)=>{window.memProbe.count++;const text=a.join(' '),at=text.indexOf('PLAYER_PROBE ');if(at>=0)try{window.memProbe.last=JSON.parse(text.slice(at+13))}catch(e){};old(...a)};return true");await click('#play');await click('#canvas');}
async function active(seconds){const begin=Date.now();let count=0,lastSnapshot=0;while(Date.now()-begin<seconds*1000){const then=Date.now(),key=count%2?'w':'s';const moves=Array.from({length:8},(_,i)=>({type:'pointerMove',duration:40,x:450+(i%2)*80,y:350+(count%2)*10,origin:'viewport'}));await call('/session/'+sid+'/actions',{actions:[{type:'pointer',id:'mouse',parameters:{pointerType:'mouse'},actions:moves},{type:'key',id:'keyboard',actions:[{type:'keyDown',value:key},{type:'pause',duration:320},{type:'keyUp',value:key}]}]});commands.push(Date.now()-then);
 if(Date.now()-lastSnapshot>10000){const p=await run('return window.memProbe');if(!p?.last)throw Error('missing real gameplay observation');snapshots.push({phase,seconds:(Date.now()-started)/1000,probe:p});lastSnapshot=Date.now();}
 if(++count%60===0)await call('/session/'+sid+'/actions',{actions:[{type:'key',id:'keyboard',actions:[{type:'keyDown',value:'v'},{type:'keyUp',value:'v'}]}]});await sleep(650);}}
async function close(){const before=readMemory();await call('/session/'+sid+'/url',{url:'about:blank'});phase+='-blank';await sleep(10000);const blank=readMemory();await call('/session/'+sid,undefined,'DELETE');sid=null;await sleep(5000);const exited=readMemory();cycles.push({before,blank,exited});
 if(blank.anon+blank.shmem>before.anon+before.shmem+64*1048576)throw Error('memory remained elevated after leaving game');if(exited.anon+exited.shmem>256*1048576)throw Error('memory remained after browser exit');}
function median(a){a.sort((a,b)=>a-b);return a[Math.floor(a.length/2)];}
(async()=>{try{
 let fd=fs.openSync(path.join(out,'server.log'),'w');server=spawn('python3',[path.join(root,'final-web/serve.py'),path.join(root,'final-web'),'--port','8804'],{stdio:['ignore',fd,fd]});fs.closeSync(fd);
 fd=fs.openSync(path.join(out,'geckodriver.log'),'w');driver=spawn('/snap/firefox/current/usr/lib/firefox/geckodriver',['--port','4446'],{stdio:['ignore',fd,fd]});fs.closeSync(fd);await sleep(1000);
 await open();phase='long-play';const activeStart=(Date.now()-started)/1000;await active(600);
 const playing=memory.filter(m=>m.phase==='long-play');const early=playing.filter(m=>m.seconds>=activeStart+60&&m.seconds<activeStart+120).map(m=>m.anon+m.shmem);const late=playing.slice(-60).map(m=>m.anon+m.shmem);const growth=median(late)-median(early);console.log('PLATEAU_GROWTH_BYTES',growth);if(growth>128*1048576)throw Error('active memory failed plateau gate');await close();
 for(let i=0;i<3;i++){phase='reopen-'+i;await open();await active(30);await close();console.log('CYCLE_COMPLETE',i);}
 if(snapshots.some((p,i)=>i&&p.phase==='long-play'&&snapshots[i-1].phase==='long-play'&&p.probe.last.sequence<=snapshots[i-1].probe.last.sequence))throw Error('gameplay stopped advancing');
 console.log('PASS endurance');save(true);
 }catch(e){errors.push(String(e.stack||e));console.error(e);save(false);process.exitCode=1;}finally{clearInterval(timer);if(sid)await call('/session/'+sid,undefined,'DELETE').catch(()=>{});if(driver)driver.kill();if(server)server.kill();}})();
