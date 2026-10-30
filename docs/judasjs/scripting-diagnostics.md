# Script diagnostics

`scripts.faults()` returns a detached snapshot of currently faulted script slots,
newest first, at most 128 records. Each `ScriptFault` contains `entity`, `slot`,
`asset` (asset ID), `callback`, `message`, and `truncated`. Text fields are limited
to 4096 UTF-8 bytes; `truncated` identifies a shortened message. Full native
records remain available in the editor's script diagnostics and stderr.

An exception in `onUI` faults that entire owning script slot, just like an
exception in another callback. Its later callbacks do not run. Other slots
continue. The query does not clear faults, restart scripts, or resume execution
at an unsafe point. A deliberately reloaded or destroyed slot disappears from
this active snapshot.

The editor displays a persistent, noninteractive fault banner with the source,
callback, message and first stack line. Experimental and Debug runtimes enable
it by default. For standalone diagnostics use `JUDAS_SCRIPT_FAULT_OVERLAY=1`;
`0` disables the runtime banner. Editor Play explicitly enables it. The banner
does not intercept input, pause the world or replace detailed diagnostics.

```js
import {scripts, console} from 'judas';
for (const fault of scripts.faults()) {
  console.log(`${fault.entity}/${fault.slot} ${fault.callback}: ${fault.message}`);
}
```

A faulty script cannot report its own fault from callbacks that no longer run.
Use another healthy script or the editor. Keep save/quit UI in a small dedicated
slot if it needs to remain available independently of gameplay failures.
