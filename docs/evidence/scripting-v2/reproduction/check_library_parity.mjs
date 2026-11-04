import fs from 'node:fs';
import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
import {loadTypeScript} from '../../../scripts/typescript_tooling.mjs';
const ts=loadTypeScript();
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../../..')+'/';
const baseline=root+'.cache/scripting-v2/baseline/source/src/';
const originalCpp=fs.readFileSync(baseline+'ScriptSystem.cpp','utf8');
const area=originalCpp.slice(originalCpp.indexOf('const std::string library='),originalCpp.indexOf('\n}\nstruct ScriptSystem::Impl'));
const original=[...area.matchAll(/R"JS\(([\s\S]*?)\)JS"/g)].map(m=>m[1]).join('')+fs.readFileSync(baseline+'ScriptUtilityLibrary.h','utf8').match(/R"JS\(([\s\S]*?)\)JS"/)[1];
const catalogue=JSON.parse(fs.readFileSync(root+'src/scripting/Modules.json','utf8'));
const next=catalogue.libraries.map(p=>fs.readFileSync(root+p.source,'utf8').match(/R"JS\(([\s\S]*?)\)JS"/)[1]).join('');

function canonical(n){
 const children=[];ts.forEachChild(n,c=>{children.push(canonical(c));});
 let text=null;
 if(ts.isNumericLiteral(n))text=Number(n.text);
 else if(ts.isIdentifier(n)||ts.isPrivateIdentifier(n)||ts.isStringLiteral(n)||ts.isNoSubstitutionTemplateLiteral(n))text=n.text;
 return [n.kind,text,children];
}
function inventory(text){
 const source=ts.createSourceFile('library.js',text,ts.ScriptTarget.ES2020,true,ts.ScriptKind.JS);
 assert.equal(source.parseDiagnostics.length,0);
 const entries=new Map();
 function member(parent,n){const kind=ts.isGetAccessor(n)?'get':ts.isSetAccessor(n)?'set':'member';const name=n.name?.text??(ts.isConstructorDeclaration(n)?'constructor':null);entries.set(parent+'.'+name+':'+kind,canonical(n));}
 for(const n of source.statements){
  if(ts.isClassDeclaration(n)){for(const m of n.members)member(n.name.text,m);}
  else if(ts.isVariableStatement(n))for(const d of n.declarationList.declarations){
   if(['extendEntity','extendWorld'].includes(d.name.text))continue;
   if(ts.isObjectLiteralExpression(d.initializer)){for(const m of d.initializer.properties)member(d.name.text,m);}
   else entries.set(d.name.text,canonical(d));
  }
  else if(ts.isExpressionStatement(n)&&ts.isCallExpression(n.expression)&&['extendEntity','extendWorld'].includes(n.expression.expression.text)){
   for(const m of n.expression.arguments[0].properties)member(n.expression.expression.text==='extendEntity'?'Entity':'world',m);
  }else entries.set('expression:'+entries.size,canonical(n));
 }
 return Object.fromEntries([...entries].sort(([a],[b])=>a.localeCompare(b)));
}
const before=inventory(original);
const after=inventory(next);
// The single global console alias is last in both logical modules; inventory
// size depends on Entity extensions, so normalize the expression key.
for(const value of [before,after])for(const key of Object.keys(value))if(key.startsWith('expression:')){value['expression']=value[key];delete value[key];}
assert.deepEqual(Object.keys(after).sort(),Object.keys(before).sort());
for(const key of Object.keys(before))assert.deepEqual(after[key],before[key],key);
process.stdout.write('Wrapper AST body parity: '+Object.keys(before).length+' declarations/members passed\n');

// A separate live JavaScript import verifies descriptors and callable arities.
globalThis.__judas=()=>null;
const old=await import('data:text/javascript;base64,'+Buffer.from(original).toString('base64'));
const updated=await import('data:text/javascript;base64,'+Buffer.from(next).toString('base64'));
assert.deepEqual(Object.keys(updated),Object.keys(old));
function describe(object){return Object.fromEntries(Object.entries(Object.getOwnPropertyDescriptors(object)).map(([key,d])=>[key,{configurable:d.configurable,enumerable:d.enumerable,writable:d.writable,kind:'value'in d?typeof d.value:'accessor',length:typeof d.value==='function'?d.value.length:null,name:typeof d.value==='function'?d.value.name:null,get:d.get?.name??null,set:d.set?.name??null}]).sort(([a],[b])=>a.localeCompare(b)));}
for(const name of Object.keys(old)){
 if(typeof old[name]==='function'&&/^class /.test(Function.prototype.toString.call(old[name])))assert.deepEqual(describe(updated[name].prototype),describe(old[name].prototype),name+' prototype');
 else if(typeof old[name]==='object')assert.deepEqual(describe(updated[name]),describe(old[name]),name+' namespace');
}
process.stdout.write('Export/prototype/property descriptor parity: '+Object.keys(old).length+' exports passed\n');
