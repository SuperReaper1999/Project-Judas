// Optional developer tooling only. Never downloads packages or searches old build caches.
import path from 'node:path';
import {createRequire} from 'node:module';
import {fileURLToPath} from 'node:url';

export function loadTypeScript(explicit, environment = process.env) {
  const require = createRequire(import.meta.url);
  const requested = explicit || environment.JUDAS_TYPESCRIPT;
  const candidates = requested ? [requested.startsWith('.') ? path.resolve(requested) : requested] : [
    'typescript',
    path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../tools/verification/node_modules/typescript'),
  ];
  const errors = [];
  for (const candidate of candidates) {
    try {
      const ts = require(candidate);
      if (typeof ts.createProgram !== 'function' || typeof ts.createSourceFile !== 'function')
        throw new Error('not a TypeScript compiler module');
      return ts;
    } catch (error) {
      errors.push(`${candidate}: ${error.message.split('\n')[0]}`);
    }
  }
  throw new Error('TypeScript is optional developer tooling and was not found. ' +
    'Install the pinned checker with npm ci --prefix tools/verification, or set ' +
    'JUDAS_TYPESCRIPT / --typescript to an installed TypeScript module. ' +
    'No engine build or game runtime requires npm.\n' + errors.join('\n'));
}
