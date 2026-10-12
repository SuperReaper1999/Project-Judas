# Native new-capture publication prototype

This is a copied diagnostic, preceding any final production repair. PhysicsWorld, WorldPersistence and PhysicalBalance replacements were explicitly linked before the untouched `791edc06…` final archive. Original BalanceLab and controller objects, timestep, inputs, gains and acceptance checks are unchanged. Link maps and `/VERBOSE:LIB` receipts confirm the original replaced archive members were not extracted.

| Execution | Actual result |
|---|---|
| Original fresh cold-write | 24 checks, 0 failures |
| Original fresh cold-read of that new save | 13 checks, 0 failures; actual supported standing |
| Original standalone controller | 119 checks, 0 failures |
| Unchanged legacy a106 cold-read | 13 checks, 1 failure |

New native save SHA256: `3346e5b6ed579bfe28eacbbe355b5b1936f1815108a92ea86fcd8ed61a4479b9`. Original a106 save remains byte-identical. No old save was rewritten or repaired. The prototype stores actual completed observation publication and actual bounded articulated continuation state; it does not fabricate contacts. Standalone History waiting behaviour remains covered by the unchanged controller test.

The first link attempt encountered safely retired original fixture objects. Their exact archived SHA-verified bytes were restored, and the three successful prototype compilations were reused. All validation ran once. PowerShell metadata expanded the first final-summary receipt beyond the HTTP16MiB upload cap; an explicit string/read-only receipt recovery completed without rerunning any test. Both failures are retained.

MSVC emitted prototype warnings, including a 64-bit EntityId narrowing in an observation-map key. The generic production version must retain full identifier width; this diagnostic establishes the tested fixture mechanism only.

`NATIVE_NEW_CAPTURE_PUBLICATION_PROOF.json` contains actual identities and results. Raw maps are losslessly compressed. Generated executables/objects and the original oversized receipt are independently preserved in ignored host storage before owned guest cleanup. These receipts must not be labelled final production acceptance.
