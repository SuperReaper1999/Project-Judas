// Run only after the serial build has completed. One browser/page/context,
// fixed Wasm heap and a hard outer cgroup/time limit; no persistent profile.
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {chromium, firefox} = require(process.env.PLAYWRIGHT_MODULE ||
  path.resolve('.cache/browser-tests/node_modules/playwright'));
const folder = path.resolve(process.env.JUDAS_RENDERER_WEB_OUTPUT || '.cache/renderer-v2/web-proof'), name = process.argv[2] || 'chromium';
if (!['chromium','firefox'].includes(name)) throw Error('Choose chromium or firefox');
let browser, server, observation;
const consoleRows = [], errors = [];
(async () => {
  if (!fs.existsSync(path.join(folder,'BUILD.json'))) throw Error('Build receipt is missing');
  const build = JSON.parse(fs.readFileSync(path.join(folder,'BUILD.json'),'utf8'));
  if (build.exitCode || !build.unchangedDuringBuild) throw Error('Build receipt is not qualified');
  server = http.createServer((request,response) => {
    const file = new URL(request.url, 'http://localhost').pathname;
    const allowed = {'/':'index.html','/index.html':'index.html','/proof.js':'proof.js','/proof.wasm':'proof.wasm'};
    if (!allowed[file]) {response.writeHead(404);response.end();return;}
    response.setHeader('Content-Type',file.endsWith('.wasm')?'application/wasm':file.endsWith('.js')?'text/javascript':'text/html');
    response.setHeader('Cache-Control','no-store');
    fs.createReadStream(path.join(folder,allowed[file])).pipe(response);
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  browser = await ({chromium,firefox}[name]).launch({headless:true,timeout:15000,
    ...(name==='chromium'?{channel:'chromium',args:[
      '--no-sandbox','--enable-unsafe-swiftshader','--use-gl=angle','--use-angle=swiftshader',
      '--disable-dev-shm-usage','--disable-background-networking','--disable-extensions',
      '--renderer-process-limit=1','--js-flags=--max-old-space-size=128']}:{})});
  const page = await browser.newPage({viewport:{width:1000,height:850}});
  page.on('console',message=>consoleRows.push({kind:message.type(),text:message.text()}));
  page.on('pageerror',error=>errors.push(error.stack||String(error)));
  await page.goto('http://127.0.0.1:'+server.address().port+'/',{timeout:15000});
  await page.waitForFunction(()=>Boolean(window.rendererPortableResult),null,{timeout:25000});
  const result = await page.evaluate(()=>window.rendererPortableResult);
  observation = {browser:name,browserVersion:browser.version(),build,result,pageErrors:errors};
  fs.writeFileSync(path.join(folder,'RESULTS-'+name+'.json'),JSON.stringify(observation,null,2)+'\n');
  await page.screenshot({path:path.join(folder,'RESULTS-'+name+'.png')});
  console.log(JSON.stringify(observation,null,2));
  if (!result.passed || result.failures || !result.contextDestroyed ||
      result.heapBytes!==33554432 || errors.length) process.exitCode=1;
})().catch(error=>{
  console.error(error.stack||String(error));process.exitCode=1;
  fs.writeFileSync(path.join(folder,'RESULTS-'+name+'.json'),JSON.stringify({
    ...(observation||{browser:name}),observerFailure:error.stack||String(error),pageErrors:errors
  },null,2)+'\n');
})
.finally(async()=>{
  fs.writeFileSync(path.join(folder,'console-'+name+'.json'),JSON.stringify({consoleRows,errors},null,2)+'\n');
  if(browser)await browser.close();
  if(server)await new Promise(resolve=>server.close(resolve));
});
