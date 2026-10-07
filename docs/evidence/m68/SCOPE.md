# M68 first-stage decisions

Starting tree: `52828779993843d9c8b0060f5e30e49e61d0466f` (PR #11 is baseline).
Measurements use Release, serial heavy captures, unchanged OS caches, owned copies.
Raw baseline captures are retained in `.cache/m68-baseline`; portable result summaries
are copied here at handoff. M67 human acceptance remains **provisional/deferred**.

| Track | Current mechanism / reproduction | Baseline cost | Acceptance before fixes | Decision |
|---|---|---|---|---|
| Runtime | Original inert reproduction, 0/400/800/6400 scenery; 0/1/8 actual script instances; 1000 steps, three processes | 800 + one script ~813 ms; 6400 + one ~12.13 s, versus no-script ~2.28 s; startup included | No per-step identity rebuild from motion; zero inert definition copies in script/event phases; callback counts and mutation safety unchanged; compare steady cost separately | Remove coarse-motion index invalidation, full-definition event snapshots, oversized callback snapshots and motor discovery scans |
| Streaming | 4/128 authored regions, fixed 1 resident vs 4 resident, 100 objects/region; repeated out/back and cancellation | Native capture in progress | No whole-world definition copies at steady residency; preserve coherent publication/pins/retained state; measure aggregate dispatch against existing 2 ms budget | Reuse membership version for ID snapshots; read live definitions and declared script slots; measure remaining native units before deciding on more work |
| Import/resource | Same local-only original M66 rig; missing products/cache and unchanged products; three processes | Cold median 12.34 s / 1.33 GiB peak; unchanged 6.383 s / 838 MiB; resource first-use separate | Unchanged products verified by content/settings/revision without repeated full decode; corruption/changed dependencies fail safely; no source quality reduction | Validated receipt fast path with embedded provenance fallback; avoid CPU pixel copies at resource handoff; trace first-use remaining costs |
| Package | Same small, Workshop, professional projects, all registered assets | Workshop ~111 ms; professional ~7.00 s / 828 MiB; bytes inventory at handoff | Default conservative dynamic lookup retained; explicit dependency roots can exclude unrelated data; reports and deterministic dependency set; moved/cold-save works | Add opt-in closure plus explicit runtime roots, preserve fingerprinted script/UI assets, inclusion report and immutable payload deduplication |
| Bounded M67 follow-through | Same 364-object Workshop, 32 selections, 31 warm edits/reloads plus first operation | Native authoring JSON retained | 200 revisions, exact undo/prefab/ID semantics and validation preserved; remove duplicate work only | Consolidate repeated candidate validation and no-change comparison; measure larger/long-history envelope |

These are M68 plans based on current captures, not claims of completed improvement.
PostDeliver's earlier lookup/VM/streaming repairs are not attributed to M68.
