// Compile the same positive/negative fixture against the preserved old and current declarations.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {loadTypeScript} from '../../../scripts/typescript_tooling.mjs';
const root=fileURLToPath(new URL('../../../',import.meta.url));
const ts=loadTypeScript();
const declaration=path.join(root,'docs/judas.d.ts');
const fixture=path.join(root,'docs/judasjs/type-contracts.ts');
const old=fs.readFileSync(new URL('baseline-judas.d.ts',import.meta.url),'utf8');
const options={noEmit:true,strict:true,target:ts.ScriptTarget.ES2020,lib:['lib.es2020.d.ts'],module:ts.ModuleKind.ESNext,moduleResolution:ts.ModuleResolutionKind.Bundler};
const compile=override=>{const host=ts.createCompilerHost(options);const read=host.readFile.bind(host);host.readFile=file=>path.resolve(file)===declaration&&override!==undefined?override:read(file);const p=ts.createProgram([declaration,fixture],options,host);return ts.getPreEmitDiagnostics(p).map(d=>({code:d.code,line:d.file?d.file.getLineAndCharacterOfPosition(d.start).line+1:null,message:ts.flattenDiagnosticMessageText(d.messageText,' ')}));};
const current=compile(),original=compile(old);
if(current.length||original.filter(d=>d.code===2345).length!==3||original.filter(d=>d.code===2578).length!==1)throw Error('Type contract positive/negative comparison changed');
console.log(JSON.stringify({current,original,pass:true},null,2));
