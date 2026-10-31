// Actual shell CSS with independent native browser scrolling/touch observations.
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('fs'),path=require('path'),assert=require('assert');
const shell=fs.readFileSync(process.argv[2]||path.join(__dirname,'../../platform/browser/shell.html'),'utf8');
const fixture=shell.split('<script src="game-config.js">')[0]+'<div id="outside" style="height:1600px;background:#223">Host page: ordinary scrolling remains available</div></body></html>';
const wait=ms=>new Promise(r=>setTimeout(r,ms));let browser;
(async()=>{
 browser=await chromium.launch({headless:true,channel:'chromium',args:['--no-sandbox']});
 const context=await browser.newContext({viewport:{width:390,height:640},isMobile:true,hasTouch:true,deviceScaleFactor:2});
 const p=await context.newPage();await p.setContent(fixture);
 await p.evaluate(()=>{document.querySelector('#loading')?.setAttribute('hidden','');window.moves=0;document.querySelector('#canvas').addEventListener('touchmove',()=>window.moves++,{passive:true});document.querySelector('#viewport').scrollIntoView()});
 const session=await context.newCDPSession(p);
 async function drag(x,y,endY){
  await session.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y,id:1}]});
  for(let n=1;n<=12;n++){await session.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x,y:y+(endY-y)*n/12,id:1}]});await wait(20)}
  await session.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await wait(350);
 }
 const box=await p.locator('#canvas').boundingBox(),before=await p.evaluate(()=>scrollY);
 await drag(box.x+box.width/2,box.y+box.height-40,box.y+40);
 const game=await p.evaluate(()=>({scroll:scrollY,moves}));
 assert(game.moves>0,'canvas must continue receiving real touch-move events');
 assert(Math.abs(game.scroll-before)<1,'dragging game canvas scrolled the host page: '+JSON.stringify({before,game}));
 await p.evaluate(()=>document.querySelector('#outside').scrollIntoView());
 const outsideBefore=await p.evaluate(()=>scrollY);await drag(190,480,140);
 const outsideAfter=await p.evaluate(()=>scrollY);
 assert(outsideAfter-outsideBefore>20,'scrolling outside the canvas was accidentally blocked');
 console.log(JSON.stringify({passed:true,canvasTouches:game.moves,canvasScrollDelta:game.scroll-before,outsideScrollDelta:outsideAfter-outsideBefore}));
})().catch(e=>{console.error(e);process.exitCode=1}).finally(async()=>{if(browser)await browser.close()});
