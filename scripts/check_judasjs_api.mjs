#!/usr/bin/env node
// Developer-only AST/typing/drift verification. No engine/runtime dependencies.
import fs from 'node:fs';
import path from 'node:path';
import {createRequire} from 'node:module';
const require=createRequire(import.meta.url);
const args=process.argv.slice(2);
const option=n=>{const i=args.indexOf(n);return i<0?undefined:args[i+1];};
const ts=require(option('--typescript')||process.env.JUDAS_TYPESCRIPT||'typescript');
const root=path.resolve(path.dirname(new URL(import.meta.url).pathname),'..');
const source=fs.readFileSync(path.join(root,'src/ScriptSystem.cpp'),'utf8');
const library=source.match(/const char\* library=R"JS\(([\s\S]*?)\)JS";/)?.[1];
if(!library)throw Error('Cannot locate registered public module: review extractor');
const parse=(name,text,kind)=>ts.createSourceFile(name,text,ts.ScriptTarget.ES2020,true,kind);
const rt=parse('judas-runtime.js',library,ts.ScriptKind.JS);
const dtsPath=path.join(root,'docs/judas.d.ts');
const decl=parse(dtsPath,fs.readFileSync(dtsPath,'utf8'),ts.ScriptKind.TS);
const runtime=new Map(),types=new Map();
const name=n=>n?.text??n?.getText();
const insert=(map,key,kind,ops=[],loc=null)=>{
 const v=map.get(key)||{symbol:key,kinds:[],native:[],line:loc?loc.getSourceFile().getLineAndCharacterOfPosition(loc.getStart()).line+1:0};
 if(!v.kinds.includes(kind))v.kinds.push(kind);
 v.native=[...new Set([...v.native,...ops])].sort();map.set(key,v);
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
const pages={Fracture:'fracture.md',Deformable:'deformables.md',saves:'saves.md',audio:'audio.md',localization:"localization.md",Material:"materials.md",profiler:"profiling.md",LiquidVolume:'liquid.md',liquid:'liquid.md',NavigationAgent:'navigation.md',navigation:'navigation.md',Entity:'entities.md',entity:'entities.md',world:'entities.md',Character:'character.md',Ragdoll:'animation-ragdolls.md',Animation:'animation-ragdolls.md',Joint:'physics.md',physics:'physics.md',scenes:'scenes-state.md',session:'scenes-state.md',input:'input.md',time:'input.md',console:'input.md',UIElement:'ui.md',UIDocument:'ui.md',ui:'ui.md'};
const abilities=k=>k.includes('field')?['get','set']:k.slice().sort();
const symbols=[...runtime.keys()].sort();
const nativeOps=[...new Set([...library.matchAll(/call\(['"]([^'"]+)['"]/g)].map(m=>m[1]))].sort();
for(const op of nativeOps)if(!source.slice(source.indexOf('JSValue ScriptSystem::Impl::Native')).match(new RegExp('op\\s*(?:==|!=)\\s*"'+op+'"')))throw Error('Unimplemented native operation '+op);
const mapPath=path.join(root,'docs/judasjs/api-inventory.json');
const schema={checkpoint:'3e5147a4ed97102200da91b4181c97b2a98942ba',authority:'src/ScriptSystem.cpp::library + Impl::Native',symbols:symbols.map(k=>{
 const v=runtime.get(k);let native=v.native;
 if(k.startsWith('physics.')&&!['physics.joint','physics.closestPoint','physics.gravity','physics.createJoint'].includes(k))native=['cast'];
 const streaming=k.startsWith('scenes.')&&!['scenes.current','scenes.registered','scenes.load','scenes.reload'].includes(k);
 const audioMember=k.startsWith('Entity.')&&(k.includes('Audio')||k==='Entity.audio');
 const page=audioMember?'audio.md':streaming?'streaming.md':['world.appearance','world.setAppearance','Entity.material'].includes(k)?'materials.md':['world.setView','world.clearView','world.viewRay','world.fluidSample'].includes(k)?'effects-camera.md':pages[k.split('.')[0]];
 return {symbol:k,kinds:v.kinds.slice().sort(),native,type:'docs/judas.d.ts:'+k,reference:'docs/judasjs/'+page};}),nativeOperations:nativeOps,
 callbacks:['onFracture','start','restore','update','fixedUpdate','uiUpdate','presentationUpdate','destroy','onUI','onCollisionEnter','onCollisionStay','onCollisionExit','onTriggerEnter','onTriggerStay','onTriggerExit'],
 exceptions:{'globalThis.__judas':'Internal native dispatcher; unsupported, not a public API declaration.','globalThis.console':'Alias of exported console; no extra API.','result objects / config / property schema':'Native structured fields (including JointConfiguration.rotationalResistance) and value validation reviewed manually; representative runtime/type examples cover shapes, not every invalid value.','types-only exports':'Interfaces/type aliases are tooling only; runtime export comparison excludes them.','setter-only accessors':'TypeScript cannot prohibit reads; docs state these return undefined.','callback ordering / phases':'Implementation traced manually; no AST checker proves temporal semantics.'}};
if(args.includes('--write-inventory')){
 fs.writeFileSync(mapPath,JSON.stringify(schema,null,2)+'\n');
 const rows=schema.symbols.map(s=>`| \`${s.symbol}\` | ${s.native.map(n=>'`'+n+'`').join(', ')||'JS wrapper/data'} | [declaration](../judas.d.ts) | [reference](${path.basename(s.reference)}) |`);
 fs.writeFileSync(path.join(root,'docs/judasjs/API_INVENTORY.md'),'# Current public JudasJS inventory\n\nM66 import review of the registered virtual module based on starting checkpoint `'+schema.checkpoint+'`.\nEach row is a runtime export/member (constructors and plain handle fields included).\nNative dispatcher operations are implementation details, not additional JS APIs.\n\n| Runtime symbol | Native bridge operation | Type | Reference |\n|---|---|---|---|\n'+rows.join('\n')+'\n\n## Lifecycle and dynamic exceptions\n\n'+schema.callbacks.map(n=>'`'+n+'`').join(', ')+' are structural ScriptBehaviour callbacks, not module exports.\n\n'+Object.entries(schema.exceptions).map(([k,v])=>'- **'+k+'**: '+v).join('\n')+'\n\nThe machine check fails missing/phantom exports/members, getter/setter drift and inventory drift.\nStructured return/configuration types and behavioural semantics require source review; it is not full semantic certification.\n');
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
const stored=JSON.parse(fs.readFileSync(mapPath,'utf8'));
if(JSON.stringify(stored)!==JSON.stringify(schema))throw Error('Inventory drift; review actual runtime/type/reference before updating');
for(const row of stored.symbols)if(!fs.existsSync(path.join(root,row.reference)))throw Error('Missing reference '+row.reference);
const callbackInterface=decl.statements.flatMap(n=>ts.isModuleDeclaration(n)?n.body.statements:[]).find(n=>ts.isInterfaceDeclaration(n)&&name(n.name)==='ScriptBehaviour');
if(!callbackInterface||stored.callbacks.some(k=>!callbackInterface.members.some(m=>name(m.name)===k)))throw Error('Callback declaration missing');
const runtimeFile=option('--runtime');if(runtimeFile){const state=JSON.parse(fs.readFileSync(runtimeFile,'utf8')),rows=state.surface||state;if(JSON.stringify(rows.slice().sort())!==JSON.stringify(symbols))throw Error('Actual VM export/prototype/field enumeration differs from source inventory');}
const examples=fs.readdirSync(path.join(root,'docs/judasjs/examples')).filter(n=>n.endsWith('.js')).map(n=>path.join(root,'docs/judasjs/examples',n));
const program=ts.createProgram([dtsPath,...examples],{allowJs:true,checkJs:true,noEmit:true,strict:true,target:ts.ScriptTarget.ES2020,lib:['lib.es2020.d.ts'],module:ts.ModuleKind.ESNext,moduleResolution:ts.ModuleResolutionKind.Bundler,skipLibCheck:false});
const diagnostics=ts.getPreEmitDiagnostics(program);
if(diagnostics.length){console.error(ts.formatDiagnosticsWithColorAndContext(diagnostics,{getCanonicalFileName:n=>n,getCurrentDirectory:()=>root,getNewLine:()=> '\n'}));process.exit(1);}
console.log(JSON.stringify({publicSymbols:symbols.length,topLevelExports:rt.statements.filter(n=>n.modifiers?.some(m=>m.kind===ts.SyntaxKind.ExportKeyword)).length,nativeOperations:nativeOps.length,callbacks:stored.callbacks.length,typeChecker:ts.version,examples:examples.length,negativeDriftChecks:negativeChecks,runtimeEnumeration:!!runtimeFile,pass:true},null,2));
