# Engine build identity

[Index](../JUDASJS.md) · [Release channels](../VERSIONING.md)

```js
import { engine } from 'judas';
const info = engine.buildInfo;
console.log(`${info.release_name}: ${info.version} [${info.channel}]`);
```

`engine.buildInfo` returns a frozen, detached object containing the executable's
embedded build identity. `version` is SemVer; `release_name` is the human-facing
title. `maturity`, `channel`, `build_id`, `source_revision`, `source_tree`,
`source_dirty`, `source_fingerprint`, `source_origin`, `source_untracked_count`,
`source_untracked_fingerprint`, `platform`, `architecture`, `configuration`,
`compiler`, `compiler_version` and `schema` match `--build-info-json`.

This reads no files, opens no network connection and changes no engine/project
state. An unknown/development source retains its genuine unknown/dirty identity.
Do not use an engine version as an automatic save, mod or network compatibility
promise: those systems have their own explicit schemas and content contracts.
There is no script-accessible updater or installer.
