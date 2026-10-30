#!/usr/bin/env node
// Developer-only AST/typing/drift verification. No engine/runtime dependencies.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {loadTypeScript} from './typescript_tooling.mjs';
const args=process.argv.slice(2);
const option=n=>{const i=args.indexOf(n);return i<0?undefined:args[i+1];};
if(args.includes('--help')){
 console.log('Usage: node scripts/check_judasjs_api.mjs [--typescript MODULE] [--runtime SURFACE.json] [--write-inventory]\nOptional checker: npm ci --prefix tools/verification (no runtime dependency).');
 process.exit(0);
}
const ts=loadTypeScript(option('--typescript'));
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const source=fs.readFileSync(path.join(root,'src/ScriptSystem.cpp'),'utf8');
const nativeSource=source+'\n'+['src/ScriptInputBindings.inl','src/ScriptLocalMultiplayer.inl','src/ScriptNetwork.inl','src/ScriptBindingsUI.cpp','src/ScriptBindingsJoint.cpp'].map(file=>fs.readFileSync(path.join(root,file),'utf8')).join('\n');
const library=source.match(/const char\* library=R"JS\(([\s\S]*?)\)JS";/)?.[1];
if(!library)throw Error('Cannot locate registered public module: review extractor');
const parse=(name,text,kind)=>ts.createSourceFile(name,text,ts.ScriptTarget.ES2020,true,kind);
const rt=parse('judas-runtime.js',library,ts.ScriptKind.JS);
const dtsPath=path.join(root,'docs/judas.d.ts');
const decl=parse(dtsPath,fs.readFileSync(dtsPath,'utf8'),ts.ScriptKind.TS);
const runtime=new Map(),types=new Map();
const name=n=>n?.text??n?.getText();
// Compare callable boundaries as well as member names. This detects a wrapper
// dropping/adding forwarded arguments; native value validation still needs tests.
const signature = (node, map) => {
 if (node && (ts.isVariableDeclaration(node) || ts.isPropertyAssignment(node))) node = node.initializer;
 if (node && ts.isShorthandPropertyAssignment(node)) return map.get(name(node.name))?.signatures?.[0];
 if (node?.type && ts.isTypeQueryNode(node.type)) return map.get(name(node.type.exprName))?.signatures?.[0];
 if (!node?.parameters) return null;
 return {
  minimum: node.parameters.filter(p => !p.initializer && !p.questionToken && !p.dotDotDotToken).length,
  maximum: node.parameters.some(p => p.dotDotDotToken) ? null : node.parameters.length
 };
};
const insert=(map,key,kind,ops=[],loc=null)=>{
 const v=map.get(key)||{symbol:key,kinds:[],native:[],line:loc?loc.getSourceFile().getLineAndCharacterOfPosition(loc.getStart()).line+1:0};
 if(!v.kinds.includes(kind))v.kinds.push(kind);
 v.native=[...new Set([...v.native,...ops])].sort();
 if (kind === 'call' || kind === 'function') {
  const current = signature(loc, map);
  if (current) {
   v.signatures ??= [];
   if (!v.signatures.some(s => s.minimum === current.minimum && s.maximum === current.maximum)) v.signatures.push(current);
  }
 }
 map.set(key,v);
};
const ops=n=>[...n.getText().matchAll(/call\(['"]([^'"]+)['"]/g)].map(m=>m[1]);
function runtimeMembers(parent,members){for(const m of members){
 let key=name(m.name);
 if(ts.isConstructorDeclaration(m)){
  insert(runtime,parent+'.constructor','call',ops(m),m);
  const visit=n=>{if(ts.isBinaryExpression(n)&&n.operatorToken.kind===ts.SyntaxKind.EqualsToken&&ts.isPropertyAccessExpression(n.left)&&n.left.expression.kind===ts.SyntaxKind.ThisKeyword)insert(runtime,parent+'.'+n.left.name.text,'field',[],n);ts.forEachChild(n,visit);};visit(m);continue;
 }
 if(!key)throw Error('Unknown runtime member');
 insert(runtime,parent+'.'+key,ts.isGetAccessor(m)?'get':ts.isSetAccessor(m)?'set':(ts.isMethodDeclaration(m)||(ts.isPropertyAssignment(m)&&ts.isArrowFunction(m.initializer))||ts.isShorthandPropertyAssignment(m))?'call':'field',ops(m),m);
}}
for(const n of rt.statements){if(!n.modifiers?.some(m=>m.kind===ts.SyntaxKind.ExportKeyword))continue;
 if(ts.isClassDeclaration(n)){insert(runtime,name(n.name),'class',[],n);runtimeMembers(name(n.name),n.members);}
 else if(ts.isVariableStatement(n))for(const d of n.declarationList.declarations){
 const k=name(d.name);if(ts.isObjectLiteralExpression(d.initializer)){insert(runtime,k,'namespace',[],d);runtimeMembers(k,d.initializer.properties);}
 else {insert(runtime,k,'function',ops(d),d);}
 }else throw Error('New export form needs inventory extractor review');
}
function declarations(nodes){for(const n of nodes){
 if(ts.isModuleDeclaration(n)&&n.body&&ts.isModuleBlock(n.body)){declarations(n.body.statements);continue;}
 if(ts.isClassDeclaration(n)){const parent=name(n.name);insert(types,parent,'class',[],n);
 for(const m of n.members){const key=ts.isConstructorDeclaration(m)?'constructor':name(m.name);let kind;
 if(ts.isConstructorDeclaration(m)||ts.isMethodDeclaration(m))kind='call';else if(ts.isGetAccessor(m))kind='get';else if(ts.isSetAccessor(m))kind='set';else if(ts.isPropertyDeclaration(m))kind=m.modifiers?.some(x=>x.kind===ts.SyntaxKind.ReadonlyKeyword)?'get':'field';else throw Error('Unhandled declaration');insert(types,parent+'.'+key,kind,[],m);}}
 else if(ts.isFunctionDeclaration(n))insert(types,name(n.name),'function',[],n);
 else if(ts.isVariableStatement(n))for(const d of n.declarationList.declarations){const parent=name(d.name);if(!ts.isTypeLiteralNode(d.type))throw Error('Namespace needs explicit type literal: '+parent);insert(types,parent,'namespace',[],d);for(const m of d.type.members)insert(types,parent+'.'+name(m.name),(ts.isMethodSignature(m)||ts.isTypeQueryNode(m.type))?'call':m.modifiers?.some(x=>x.kind===ts.SyntaxKind.ReadonlyKeyword)?'get':'field',[],m);}
}}
declarations(decl.statements);
const pages={NetworkEntity:'network.md',network:'network.md',Terrain:'terrain.md',LocalPlayer:'local-multiplayer.md',PlayerInput:'local-multiplayer.md',players:'local-multiplayer.md',Weather:'weather.md',weather:'weather.md',mods:'mods.md',signals:'signals.md',Fracture:'fracture.md',Deformable:'deformables.md',saves:'saves.md',audio:'audio.md',localization:"localization.md",Material:"materials.md",profiler:"profiling.md",LiquidVolume:'liquid.md',liquid:'liquid.md',NavigationAgent:'navigation.md',navigation:'navigation.md',Entity:'entities.md',entity:'entities.md',world:'entities.md',EntityGravity:'physics.md',Character:'character.md',Wheel:'wheels.md',Powertrain:'powertrains.md',Ragdoll:'animation-ragdolls.md',Animation:'animation-ragdolls.md',Joint:'physics.md',physics:'physics.md',scenes:'scenes-state.md',session:'scenes-state.md',input:'input.md',time:'input.md',console:'input.md',UIElement:'ui.md',UIDocument:'ui.md',ui:'ui.md'};
// Component accessors and cross-cutting world operations belong at the page
// documenting their actual contract, rather than all pointing to Entity's index.
const referenceOverrides = {
 'Entity.animation': 'animation-ragdolls.md', 'Entity.ragdoll': 'animation-ragdolls.md',
 'Entity.character': 'character.md', 'Entity.deformable': 'deformables.md',
 'Entity.fracture': 'fracture.md', 'Entity.gravity': 'physics.md',
 'Entity.liquid': 'liquid.md', 'Entity.material': 'materials.md',
 'Entity.navigation': 'navigation.md', 'Entity.navigationLink': 'navigation.md',
 'Entity.navigationObstacle': 'navigation.md', 'Entity.setNavigationEnabled': 'navigation.md',
 'Entity.powertrain': 'powertrains.md', 'Entity.terrain': 'terrain.md',
 'Entity.weather': 'weather.md', 'Entity.wheel': 'wheels.md',
 'Entity.body2D':'physics-2d.md', 'physics.cast2D':'physics-2d.md', 'Entity.sprite':'sprites-2d.md', 'Entity.setSprite':'sprites-2d.md', 'world.unproject':'sprites-2d.md',
 'Entity.camera': 'effects-camera.md', 'Entity.setCameraProjection': 'effects-camera.md',
 'world.appearance': 'materials.md', 'world.setAppearance': 'materials.md',
 'world.resetAppearance': 'materials.md', 'world.setView': 'effects-camera.md',
 'world.clearView': 'effects-camera.md', 'world.viewRay': 'effects-camera.md',
 'world.fluidSample': 'effects-camera.md', 'world.project': 'effects-camera.md',
 'world.viewport': 'effects-camera.md', 'world.origin': 'planetary-terrain.md',
 'world.originState': 'planetary-terrain.md', 'world.toAbsolute': 'planetary-terrain.md',
 'world.toLocal': 'planetary-terrain.md', 'world.requestOrigin': 'planetary-terrain.md',
 'Entity.planetState': 'planetary-terrain.md', 'Entity.planetSample': 'planetary-terrain.md',
 'Entity.preparePlanetCollision': 'planetary-terrain.md', 'Entity.clearPlanetCollision': 'planetary-terrain.md'
};
function referencePage(symbol) {
 if (symbol === 'engine' || symbol.startsWith('engine.')) return 'engine.md';
 if (referenceOverrides[symbol]) return referenceOverrides[symbol];
 if (symbol.startsWith('Entity.') && (symbol.includes('Audio') || symbol === 'Entity.audio')) return 'audio.md';
 if (symbol.startsWith('scenes.') && !['scenes.current', 'scenes.registered', 'scenes.load', 'scenes.reload'].includes(symbol)) return 'streaming.md';
 const page = pages[symbol.split('.')[0]];
 if (!page) throw Error('Missing reference owner: ' + symbol);
 return page;
}
const abilities=k=>k.includes('field')?['get','set']:k.slice().sort();
const symbols=[...runtime.keys()].sort();
const nativeOps=[...new Set([...library.matchAll(/call\(['"]([^'"]+)['"]/g)].map(m=>m[1]))].sort();
for(const op of nativeOps)if(!nativeSource.slice(source.indexOf('JSValue ScriptSystem::Impl::Native')).match(new RegExp('op\\s*(?:==|!=)\\s*"'+op+'"')))throw Error('Unimplemented native operation '+op);
const mapPath=path.join(root,'docs/judasjs/api-inventory.json');
const resultShapes=[
 {binding:'Entity.sprite',declaration:'SpriteSettings',fields:['size','pivot','rect','tint','flipX','flipY','nearest','pixelSnap','alphaPick','ySort','sortingLayer','order'],nativeFile:'src/ScriptSpriteValues.inl',needles:['vector("size"','vector("pivot"','vector("rect"','vector("tint"','{"flipX",sprite.flipX}','{"flipY",sprite.flipY}','{"nearest",sprite.nearest}','{"pixelSnap",sprite.pixelSnap}','{"alphaPick",sprite.alphaPick}','{"ySort",sprite.ySort}','JS_SetPropertyStr(c,out,"sortingLayer"','JS_SetPropertyStr(c,out,"order"'],reference:'docs/judasjs/sprites-2d.md'},
 {binding:'Entity.physicalMaterial',declaration:'PhysicalMaterialInfo',fields:['asset','friction','restitution'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,o,"asset",asset.empty()?JS_NULL:JS_NewString(c,asset.c_str()))','JS_SetPropertyStr(c,o,"friction",JS_NewFloat64(c,friction))','JS_SetPropertyStr(c,o,"restitution",JS_NewFloat64(c,restitution))'],reference:'docs/judasjs/physics.md'},
 {binding:'network.stats',declaration:'NetworkStats',fields:['bytesSent','bytesReceived','framesSent','framesReceived','backpressure','coalesced','rejected','snapshots','snapshotAge','entities','peers'],nativeFile:'src/ScriptNetwork.inl',needles:['{"bytesSent",t.bytesSent}','{"bytesReceived",t.bytesReceived}','{"framesSent",t.framesSent}','{"framesReceived",t.framesReceived}','{"backpressure",t.backpressure}','{"coalesced",t.dropped}','{"rejected",stats.rejected+t.rejected}','{"snapshots",stats.snapshots}','{"snapshotAge",stats.snapshotAge}','{"entities",stats.entities}','{"peers",stats.peers}'],reference:'docs/judasjs/network.md'},
 {binding:'network.launch',declaration:'NetworkLaunch',fields:['mode','address','port','dedicated'],nativeFile:'src/ScriptNetwork.inl',needles:['{"mode",launch.mode}','{"address",launch.address}','{"port",launch.port}','{"dedicated",launch.dedicated}'],reference:'docs/judasjs/network.md'},
 {binding:'network.peers',declaration:'NetworkPeer',fields:['id','name','connected','player'],nativeFile:'src/ScriptNetwork.inl',needles:['{"id",p.id}','{"name",p.name}','{"connected",p.connected}','{"player",p.player?std::to_string(p.player):std::string()}'],reference:'docs/judasjs/network.md'},
 {binding:'saves.status',declaration:'SaveRequest',fields:['id','operation','slot','state','error','recovered','captureMs','workerMs','restoreMs','bytes','migrated','migrationMs','migrationReport'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,o,"migrated",JS_NewBool(c,status->migrated))','{"migrationMs",status->migrationMs}','{"migrationReport",&status->migrationReport}'],reference:'docs/judasjs/saves.md'},
 {binding:'saves.list',declaration:'SaveSlot',fields:['id','name','scene','timestamp','metadata','status','error','recovered','modContext','content','compatibility','contentVersion'],nativeFile:'src/ScriptSystem.cpp',needles:['{"content",&slot.content}','{"compatibility",&slot.compatibility}','JS_SetPropertyStr(c,o,"contentVersion",JS_NewUint32(c,slot.contentVersion))'],reference:'docs/judasjs/saves.md'},
 {binding:'Terrain.state',declaration:'TerrainStatus',fields:['ready','busy','navigation','revision','samples','triangles','bytes','prepareMs','publishMs','error'],nativeFile:'src/ScriptSystem.cpp',needles:['{"ready",state->ready}','{"revision",double(state->revision)}','JS_SetPropertyStr(c,result,"error",JS_NewString(c,state->error.c_str()))'],reference:'docs/judasjs/terrain.md'},
 {binding:'Animation.advance',declaration:'AnimationSample',fields:['motion','events','overflow'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,result,"motion",motion)','JS_SetPropertyStr(c,result,"events",events)','JS_SetPropertyStr(c,result,"overflow",JS_NewBool(c,step.overflow))'],reference:'docs/judasjs/animation-ragdolls.md'},
 {binding:'Animation.clips',declaration:'ClipInfo',fields:['name','duration','events'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,value,"events",markers)','JS_SetPropertyStr(c,value,"duration",JS_NewFloat64(c,clip.duration))'],reference:'docs/judasjs/animation-ragdolls.md'},
 {binding:'Entity.light',declaration:'LightSettings',fields:['kind','color','enabled','castsShadow','intensity','range','innerConeDegrees','outerConeDegrees','shadowPriority'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,out,"kind"','JS_SetPropertyStr(c,out,"color"','JS_SetPropertyStr(c,out,"enabled"','JS_SetPropertyStr(c,out,"castsShadow"','{"intensity",l.intensity}','{"range",l.range}','{"innerConeDegrees",l.innerConeDegrees}','{"outerConeDegrees",l.outerConeDegrees}','{"shadowPriority",l.shadowPriority}'],reference:'docs/judasjs/entities.md'},
 {binding:'Weather.state',declaration:'WeatherState',fields:['current','target','time','transitionElapsed','transitionDuration','tornadoPosition','lightningSequence','lightningPosition','lightningFlash'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,result,"current",weatherProfileValue(field->Current()))','JS_SetPropertyStr(c,result,"target",weatherProfileValue(state.to))','JS_SetPropertyStr(c,result,"time",JS_NewFloat64(c,state.time))','JS_SetPropertyStr(c,result,"transitionElapsed",JS_NewFloat64(c,state.transitionElapsed))','JS_SetPropertyStr(c,result,"transitionDuration",JS_NewFloat64(c,state.transitionDuration))','JS_SetPropertyStr(c,result,"lightningSequence",JS_NewString(c,std::to_string(state.lightning.sequence).c_str()))','JS_SetPropertyStr(c,result,"lightningPosition",preciseVector(state.lightning.position-world.Settings().worldOrigin))','JS_SetPropertyStr(c,result,"tornadoPosition",preciseVector(sample.tornadoPosition-world.Settings().worldOrigin))','JS_SetPropertyStr(c,result,"lightningFlash",JS_NewFloat64(c,sample.lightningFlash))'],reference:'docs/judasjs/weather.md'},
 {binding:'weather.sample',declaration:'WeatherQuery',fields:['wind','up','altitude','airDensity','rain','snow','fog','cloudCoverage','tornadoPosition','lightningFlash'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,result,"wind",preciseVector(sample.wind))','JS_SetPropertyStr(c,result,"up",preciseVector(sample.up))','JS_SetPropertyStr(c,result,"tornadoPosition",preciseVector(sample.tornadoPosition-world.Settings().worldOrigin))','{"altitude",sample.altitude}','{"airDensity",sample.airDensity}','{"rain",sample.profile.rain}','{"snow",sample.profile.snow}','{"fog",sample.profile.fog}','{"cloudCoverage",sample.profile.cloudCoverage}','{"lightningFlash",sample.lightningFlash}'],reference:'docs/judasjs/weather.md'},
 {binding:'Entity.camera',declaration:'CameraInfo',fields:['near','far','enabled','width','height','frustum','clipPlane'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,o,"frustum"','JS_SetPropertyStr(c,o,"clipPlane"'],reference:'docs/judasjs/effects-camera.md'},
 {binding:'Entity.planetSample',declaration:'PlanetSample',fields:['position','normal','height','distance'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,o,"position",preciseVector(sample.point))','JS_SetPropertyStr(c,o,"normal",preciseVector(sample.normal))','JS_SetPropertyStr(c,o,"height",JS_NewFloat64(c,sample.height))','JS_SetPropertyStr(c,o,"distance",JS_NewFloat64(c,sample.distance))'],reference:'docs/judasjs/planetary-terrain.md'},
 {binding:'Fracture.state.interfaces',declaration:'FractureInterfaceState',fields:['material','plasticTranslation','plasticRotation','plasticWork'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,v,"material"','JS_SetPropertyStr(c,v,"plasticTranslation"','JS_SetPropertyStr(c,v,"plasticRotation"','JS_SetPropertyStr(c,v,"plasticWork"'],reference:'docs/judasjs/fracture.md'},
 {binding:'Wheel.settings',declaration:'WheelSettings',fields:['part'],nativeFile:'src/ScriptSystem.cpp',needles:['JS_SetPropertyStr(c,result,"part"'],reference:'docs/judasjs/wheels.md'},
 {binding:'Powertrain.settings.links[].connection',declaration:'PowertrainConnection',fields:['groups'],nativeFile:'src/PowertrainAuthoring.cpp',needles:['connection["groups"]'],reference:'docs/judasjs/powertrains.md'}
];
function checkResultShapes(shapes){for(const row of shapes){let members=null;const visit=n=>{if(ts.isInterfaceDeclaration(n)&&name(n.name)===row.declaration)members=new Set(n.members.map(m=>name(m.name)));ts.forEachChild(n,visit);};visit(decl);if(!members||row.fields.some(k=>!members.has(k)))throw Error('Structured declaration drift: '+row.binding);const native=fs.readFileSync(path.join(root,row.nativeFile),'utf8');if(row.needles.some(k=>!native.includes(k)))throw Error('Structured native field drift: '+row.binding);}}
checkResultShapes(resultShapes);
let rejectedShape=false;try{checkResultShapes([{...resultShapes[0],fields:['inventedMaterialField']}]);}catch{rejectedShape=true;}if(!rejectedShape)throw Error('Structured field negative control accepted');
const callableExceptions = {
 'Entity.setSocket': {minimum: 1, reason: 'A null/undefined target clears the socket before the joint argument is read.'},
 'Weather.strike': {minimum: 0, reason: 'The wrapper forwards undefined; weatherStrike accepts omission and uses the field ground anchor.'}
};
const callableBounds = signatures => ({
 minimum: Math.min(...signatures.map(s => s.minimum)),
 maximum: signatures.some(s => s.maximum === null) ? null : Math.max(...signatures.map(s => s.maximum))
});
const callables = symbols.filter(k => runtime.get(k).signatures).map(symbol => ({
 symbol, ...callableBounds(runtime.get(symbol).signatures),
 ...(callableExceptions[symbol] ? {nativeOptional: callableExceptions[symbol]} : {})
}));
function checkCallables(declarations) {
 for (const row of callables) {
  const declared = declarations.get(row.symbol)?.signatures;
  if (!declared) throw Error('Missing callable signature: ' + row.symbol);
  const actual = callableBounds(declared);
  const minimum = row.nativeOptional?.minimum ?? row.minimum;
  if (actual.minimum !== minimum || actual.maximum !== row.maximum) throw Error('Callable argument drift: ' + row.symbol);
 }
}
checkCallables(types);
const schema={checkpoint:'Current public module including Experimental 2D Stage 2; source version in engine-version.json',authority:'src/ScriptSystem.cpp::library + Impl::Native + ScriptBindingsUI.cpp + ScriptBindingsJoint.cpp + ScriptInputBindings.inl + ScriptLocalMultiplayer.inl + ScriptNetwork.inl',resultShapes,callables,symbols:symbols.map(k=>{
 const v=runtime.get(k);let native=v.native;
 if(['physics.raycastMany','physics.sphereCastMany','physics.capsuleCastMany'].includes(k))native=['castMany'];
 else if(k==='physics.cast2D')native=['cast2D'];
 else if(k.startsWith('physics.')&&!['physics.joint','physics.closestPoint','physics.gravity','physics.createJoint'].includes(k))native=['cast'];
 const page=referencePage(k);
 return {symbol:k,kinds:v.kinds.slice().sort(),native,type:'docs/judas.d.ts:'+k,reference:'docs/judasjs/'+page};}),nativeOperations:nativeOps,
 callbacks:['onOriginShift','onSignal','onFracture','start','restore','update','fixedUpdate','uiUpdate','presentationUpdate','destroy','onUI','onCollisionEnter','onCollisionStay','onCollisionExit','onTriggerEnter','onTriggerStay','onTriggerExit'],
 exceptions:{'globalThis.__judas':'Internal native dispatcher; unsupported, not a public API declaration.','globalThis.console':'Alias of exported console; no extra API.','result objects / config / property schema':'Native structured fields (including JointConfiguration.rotationalResistance, M83 DeformableAttachmentOptions/observations and PowertrainConnection/connected) and value validation reviewed manually; representative runtime/type examples cover shapes, not every invalid value.','types-only exports':'Interfaces/type aliases are tooling only; runtime export comparison excludes them.','setter-only accessors':'TypeScript cannot prohibit reads; docs state these return undefined.','callback ordering / phases':'Implementation traced manually; no AST checker proves temporal semantics.','callable argument counts':'Wrapper/declaration minimum and maximum counts (including aliases, rest arguments and overload bounds) are checked. Entity.setSocket has a null-target branch and Weather.strike explicitly accepts native undefined. Parameter types, overload gaps and native validation still require source/behavioural review.'}};
if(args.includes('--write-inventory')){
 fs.writeFileSync(mapPath,JSON.stringify(schema,null,2)+'\n');
 const rows=schema.symbols.map(s=>`| \`${s.symbol}\` | ${s.native.map(n=>'`'+n+'`').join(', ')||'JS wrapper/data'} | [declaration](../judas.d.ts) | [reference](${path.basename(s.reference)}) |`);
 fs.writeFileSync(path.join(root,'docs/judasjs/API_INVENTORY.md'),'# Current public JudasJS inventory\n\nCurrent public surface identity: `'+schema.checkpoint+'`. Includes optional network sessions and NetworkEntity handles; see [networking](network.md). Human/platform acceptance is recorded separately in [the current documentation](../JUDASJS.md), not inferred from API coverage.\nEach row is a runtime export/member (constructors and plain handle fields included).\nNative dispatcher operations are implementation details, not additional JS APIs.\n\n| Runtime symbol | Native bridge operation | Type | Reference |\n|---|---|---|---|\n'+rows.join('\n')+'\n\n## Lifecycle and dynamic exceptions\n\n'+schema.callbacks.map(n=>'`'+n+'`').join(', ')+' are structural ScriptBehaviour callbacks, not module exports.\n\n'+Object.entries(schema.exceptions).map(([k,v])=>'- **'+k+'**: '+v).join('\n')+'\n\nThe machine check fails missing/phantom exports/members, getter/setter drift, callable argument-count drift and inventory drift.\nThe manifest also records callable bounds and explicit argument-count exceptions. Structured return/configuration types and behavioural semantics require source review; it is not full semantic certification.\n');
}
function checkSurface(declarations){
 const missing=symbols.filter(k=>!declarations.has(k)),phantom=[...declarations.keys()].filter(k=>!runtime.has(k));
 if(missing.length||phantom.length)throw Error(JSON.stringify({missing,phantom}));
 for(const k of symbols){const r=abilities(runtime.get(k).kinds),t=abilities(declarations.get(k).kinds);if(JSON.stringify(r)!==JSON.stringify(t))throw Error(k+' access kind mismatch '+r+' vs '+t);}
}
checkSurface(types);
let negativeChecks=0;
for(const mutation of ['missing','phantom','accessor']){
 const altered=new Map(types);
 if(mutation==='missing')altered.delete('Entity.destroy');
 if(mutation==='phantom')altered.set('world.inventedFeature',{kinds:['call']});
 if(mutation==='accessor')altered.set('Entity.transform',{kinds:['get']});
 let rejected=false;try{checkSurface(altered);}catch{rejected=true;}if(!rejected)throw Error('Drift negative control accepted '+mutation);negativeChecks++;
}
for (const mutation of ['missing-signature', 'required-argument', 'extra-argument']) {
 const altered = new Map(types);
 const target = {...altered.get('physics.raycast')};
 target.signatures = mutation === 'missing-signature' ? [] : [{minimum: mutation === 'required-argument' ? 2 : 3, maximum: mutation === 'extra-argument' ? 5 : 4}];
 if (mutation === 'missing-signature') delete target.signatures;
 altered.set('physics.raycast', target);
 let rejected = false;
 try { checkCallables(altered); } catch { rejected = true; }
 if (!rejected) throw Error('Callable negative control accepted: ' + mutation);
 negativeChecks++;
}
const stored=JSON.parse(fs.readFileSync(mapPath,'utf8'));
if(JSON.stringify(stored)!==JSON.stringify(schema))throw Error('Inventory drift; review actual runtime/type/reference before updating');
for(const row of stored.symbols)if(!fs.existsSync(path.join(root,row.reference)))throw Error('Missing reference '+row.reference);
const callbackInterface=decl.statements.flatMap(n=>ts.isModuleDeclaration(n)?n.body.statements:[]).find(n=>ts.isInterfaceDeclaration(n)&&name(n.name)==='ScriptBehaviour');
if(!callbackInterface||stored.callbacks.some(k=>!callbackInterface.members.some(m=>name(m.name)===k)))throw Error('Callback declaration missing');
const runtimeFile=option('--runtime');if(runtimeFile){const state=JSON.parse(fs.readFileSync(runtimeFile,'utf8')),rows=state.surface||state;if(JSON.stringify(rows.slice().sort())!==JSON.stringify(symbols))throw Error('Actual VM export/prototype/field enumeration differs from source inventory');}
const examples=fs.readdirSync(path.join(root,'docs/judasjs/examples')).filter(n=>n.endsWith('.js')).map(n=>path.join(root,'docs/judasjs/examples',n));
const typeContracts=path.join(root,'docs/judasjs/type-contracts.ts');
const program=ts.createProgram([dtsPath,typeContracts,...examples],{allowJs:true,checkJs:true,noEmit:true,strict:true,target:ts.ScriptTarget.ES2020,lib:['lib.es2020.d.ts'],module:ts.ModuleKind.ESNext,moduleResolution:ts.ModuleResolutionKind.Bundler,skipLibCheck:false});
const diagnostics=ts.getPreEmitDiagnostics(program);
if(diagnostics.length){console.error(ts.formatDiagnosticsWithColorAndContext(diagnostics,{getCanonicalFileName:n=>n,getCurrentDirectory:()=>root,getNewLine:()=> '\n'}));process.exit(1);}
console.log(JSON.stringify({publicSymbols:symbols.length,topLevelExports:rt.statements.filter(n=>n.modifiers?.some(m=>m.kind===ts.SyntaxKind.ExportKeyword)).length,nativeOperations:nativeOps.length,callbacks:stored.callbacks.length,typeChecker:ts.version,examples:examples.length,negativeDriftChecks:negativeChecks+1,structuredResultShapes:resultShapes.length,callableSignatures:callables.length,typeContractFixture:true,runtimeEnumeration:!!runtimeFile,pass:true},null,2));
