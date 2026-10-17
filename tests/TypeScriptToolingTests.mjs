import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import {loadTypeScript} from '../scripts/typescript_tooling.mjs';

test('explicit missing compiler fails with an actionable message and no fallback', () => {
  assert.throws(() => loadTypeScript('/absent/judas-typescript-checker', {}),
    /npm ci --prefix tools\/verification/);
});
test('explicit malformed compiler is rejected instead of pretending tooling passed', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'judas tooling ü '));
  try {
    const module = path.join(root, 'compiler.cjs');
    fs.writeFileSync(module, 'module.exports={version:"fake"};');
    assert.throws(() => loadTypeScript(module, {}), /not a TypeScript compiler module/);
  } finally { fs.rmSync(root, {recursive:true}); }
});
test('environment module handles spaces and Unicode, independently of milestone caches', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'judas tooling ü '));
  try {
    const module = path.join(root, 'compiler.cjs');
    fs.writeFileSync(module, 'module.exports={createProgram(){},createSourceFile(){},version:"fixture"};');
    assert.equal(loadTypeScript(undefined, {JUDAS_TYPESCRIPT:module}).version, 'fixture');
  } finally { fs.rmSync(root, {recursive:true}); }
});
test('real installed compiler parses a declaration', () => {
  const ts = loadTypeScript();
  const source = ts.createSourceFile('probe.d.ts', 'declare const example: number;', ts.ScriptTarget.ES2020);
  assert.equal(source.parseDiagnostics.length, 0);
  assert.equal(source.statements.length, 1);
});
