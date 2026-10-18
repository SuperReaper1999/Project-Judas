# Pass 5: bounded contact-subcost observer

## Scope and physical proof

This is a **cache-only, two-translation-unit diagnostic experiment**, using the
already validated fat-candidate cache. Production source is unchanged. It runs the
original 270-step room1024 fixture once, with every original geometry operation,
CCD setting and solver action retained. All 270 records / 15 physical fields and
endpoint geometry match the accepted-4c baseline exactly. Only endpoint observation
timing differs. The binary completes successfully; observer overhead is excluded
from accepted performance comparisons.

`probe/SOURCES.json` identifies exact production inputs and generated diagnostic
copies. `probe/BUILD.json` records commands, flags, archive/binary hashes and
provenance. It replaces only `WorldImpactEvents.cpp.o` and `Contacts.cpp.o` in a
copy of the frozen accepted engine archive. The actual diagnostic payload is not
represented by the embedded baseline/dirty build label alone; use these hashes.
No executables, archives or objects are committed.

## Attribution in physical step 270

| Rich PairAt category | Discovery | Response |
|---|---:|---:|
| Calls | 467747 | 52420 |
| Actual AABB rejection | 342322 | 0 |
| Hits | 77799 | 52420 |
| Misses | 389948 | 0 |
| Uncertain | 0 | 0 |
| Canonical body-order hits | 25379 | 52420 |
| Reverse body-order hits | 52420 | 0 |
| Context constructions | 125425 | 52420 |
| Box SAT calls | 56725 | 24896 |
| SAT axes evaluated | 579529 | 373440 |
| Cross-orientation rows prepared | 146738 | 74688 |
| Face witness/clipping constructions | 33275 | 24543 |
| Edge witness constructions | 776 | 353 |
| Eight-corner fallback iterations | 0 | 0 |
| Full PairAt diagnostic ms | 485.131 | 209.986 |
| Context construction diagnostic ms | 35.835 | 16.954 |
| Box SAT diagnostic ms | 252.805 | 148.992 |
| Post-SAT witness diagnostic ms | 19.111 | 13.087 |

Timers are nested, include counter overhead and do not establish uninstrumented
production subcosts. They show where the diagnostic run spends its geometry work.
The wrapper categorizes the actual-AABB counter delta, so those early rejections
are not mistaken for full SAT misses. SAT, construction and witness costs are
measured within rich discovery/response calls; other CCD/Boolean callers are not
independently timed by this probe. No exact-sign or numeric-gap fallback occurs in
these categorized calls. Raw counts/timings for all steps are retained.

## Established duplication

A reverse-order simple-primitive hit is used only as a Boolean discovery result.
The current code then performs a canonical-order rich query to build response
rows. At step 270 this accounts for **all 52,420 additional response calls**.
Across the full run it accounts for **535,542** additional calls and 2,144.242 ms
of diagnostic response time. Response traversal and constraints are intentionally
canonical today; flipping a discovered manifold's rows would change their
arithmetic and is not an accepted substitute.

An ordinary broad exact-pose memo still has low potential: the observed directional
query/pose repeats remain rare. Caching prepared parent orientations already works.
The source does not rebuild eight native box vertices for every pair: positive
face contacts clip four incident polygon vertices; the eight-corner fallback is
not exercised here. The measured interval/SAT work is substantially larger than
post-SAT witness construction, so optimizing clipping alone is unlikely to remove
the tail.

## Next correction boundary, not a shipped change

A useful next experiment is to obtain the existing **canonical response manifold**
during simple-primitive discovery and retain it for response even when the pair was
encountered in reverse. Traversal and response-row order must remain unchanged.
The accepted-contact existence and uncertainty result must first be proved
consistent with the old directional query, including near separation, tangency,
rotation, offsets, malformed/range inputs and compounds. A canonical valid result
alone does not prove that a differently ordered SAT/numeric path cannot report
uncertainty. If equivalence cannot be established, keep the original path or limit
reuse to a strictly justified subset. No production patch is included in this
receipt and no speedup is claimed for that proposed change.

The severe 1.7–2.0 second dense-step limit remains open. This diagnostic narrows
further work to a demonstrated duplicate, while preserving every original physical
acceptance condition.
