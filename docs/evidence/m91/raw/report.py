from pathlib import Path
import json,csv,statistics,math
r=Path.cwd();c=r/'.cache/m91';d=r/'docs/evidence/m91';d.mkdir(exist_ok=True)
load=lambda p:json.loads((c/p).read_text())
base=load('final-dense/RESULTS.json');dense=load('dense-verified/RESULTS.json');profile=load('profile-verified/RESULTS.json');consumer=load('consumer-verified/results.json');native=load('verified-native/RESULTS.json');cold=load('verified-cold/RECEIPT.json');delivery=load('verified-delivery/RESULTS.json');scaling=load('scaling-verified-repeated/RESULTS.json')
f=lambda v:f'{v:.3f}'
rows=[]
for case in ['play','coins']:
 b=next(x for x in consumer['runs']if x['label']=='baseline'and x['mode']==case);a=next(x for x in consumer['runs']if x['label']=='candidate'and x['mode']==case)
 rows.append(f"| Tuppence {case}, 1048 fixed samples | {f(b['fixed_ms']['median'])} / {f(b['fixed_ms']['p95'])} / {f(b['fixed_ms']['maximum'])} | {f(a['fixed_ms']['median'])} / {f(a['fixed_ms']['p95'])} / {f(a['fixed_ms']['maximum'])} |")
b=[x for x in base['runs']if x['name'].startswith('before-155')];a=dense['runs'][0]
rows.append(f"| M87 1024 pile, 155 steps | {f(statistics.median(x['fixed_ms']['median']for x in b))} / {f(statistics.median(x['fixed_ms']['p95_nearest_rank']for x in b))} / {f(max(x['fixed_ms']['maximum']for x in b))} | {f(a['fixed_ms']['median'])} / {f(a['fixed_ms']['p95_nearest_rank'])} / {f(a['fixed_ms']['maximum'])} |")
for w in ['free','pile','mesh','machines_sleeping']:
 def v(label):
  runs=[x for x in scaling['runs']if x['label']==label and x['workload']==w]
  return ' / '.join(f(statistics.median(x['summary']['input_and_step_ms'][k]for x in runs))for k in ['median','p95','maximum'])
 rows.append(f'| M87 {w}, 1024 bodies | {v("before")} | {v("after")} |')
concave={}
for label,name in [('before','concave/before.log'),('after','concave/after-verified.log')]:
 concave[label]=[json.loads(line)for line in(c/name).read_text().splitlines()if line.startswith('{')]
b=next(x for x in concave['before']if x['case']=='mesh-4');a=next(x for x in concave['after']if x['case']=='mesh-4')
rows.append(f"| Four imported 13-body rigs, 8192 triangles, 900 steps | {f(b['medianMs'])} / {f(b['p95Ms'])} / {f(b['maxMs'])} | {f(a['medianMs'])} / {f(a['p95Ms'])} / {f(a['maxMs'])} |")
watchRows=list(csv.DictReader((c/'watch-verified/frames.csv').open()));watchSteps=list(csv.DictReader((c/'watch-verified/steps.csv').open()))
def distribution(values):
 values=sorted(values);return {'median':values[len(values)//2],'p95_nearest_rank':values[math.ceil(.95*len(values))-1],'maximum':values[-1]}
watch={'frames':len(watchRows),'steps':len(watchSteps),'wall_ms':float(watchRows[-1]['wall_ms']),'frame_interval_ms':distribution([float(x['interval_ms'])for x in watchRows[1:]]),'fixed_ms':distribution([float(x['fixed_ms'])for x in watchSteps]),'maximum_fixed_steps_per_frame':max(int(x['fixed_steps'])for x in watchRows)};(c/'watch-verified/SUMMARY.json').write_text(json.dumps(watch,indent=2)+'\n')
comparison={'baseline_dense_receipt':'raw/final-dense/RESULTS.json','final_dense_receipt':'raw/dense-verified/RESULTS.json','final_profile_receipt':'raw/profile-verified/RESULTS.json','consumer':consumer['runs'],'concave':concave,'watch':watch,'native_pass':native['pass'],'cold_pass':cold['pass'],'delivery_pass':delivery['pass'],'scaling_pass':scaling['pass']};(d/'RESULTS.json').write_text(json.dumps(comparison,indent=2)+'\n')
long=load('closed-long/RESULTS.json')['runs'][0];a=dense['runs'][1];pr=profile['runs'][0]
px=load('profile-verified/after-270-r0/profile-shutdown.json');pf=max(px['frames'],key=lambda t:t['end_ns']-t['start_ns']);pc={x['name']:x['value']for x in pf['counters']}
q0=next(x for x in base['runs']if x['name']=='before-155-r0')['endpoint_quality'];q1=dense['runs'][0]['endpoint_quality'];q2=a['endpoint_quality'];bp=next(x for x in consumer['runs']if x['label']=='baseline'and x['mode']=='play');ap=next(x for x in consumer['runs']if x['label']=='candidate'and x['mode']=='play')
text=f'''# M91 — Predictable Collision at Scale: measured implementation and limits

## Status and scope

Linux-validated collision improvements for **Experimental**; human review pending.
Main is not promoted. This is **not closure of all dense-contact/CCD stalls**:
long, globally connected piles remain a severe performance limit. No universal
60 Hz deadline or complete continuous-collision proof is claimed.

- Accepted starting source: `4628494c4edcbad1654db21da21b392a9bb1f987`.
- Human-tested main retained at `1f37b6b1b91c86d17646a4f0e8165f8112af9e08`.
- Native Linux / Intel i7-4790, four cores/eight threads; GTX 970 4 GB,
  NVIDIA 580.178.04. Release `-O3 -DNDEBUG`; incremental affected-target build,
  zero compiler warnings. The unchanged preserved concave observer has its
  original misleading-indentation warnings; no historical source was edited.
- Actual UTC timestamps, configuration, source and measured binary identities:
  [baseline](raw/baseline/SOURCE.json), [final fingerprints](FINAL_FINGERPRINTS.json),
  [binary fingerprints](FINAL_BINARIES.json), [reproduction](REPRODUCTION.md).
- No full production-suite rerun. Windows/Android were not run for M91; neither
  platform is certified by Linux results. Windows remains informational.

## Measured cause and architecture

M87's preserved failure is a real low-speed event cascade, not a zero-time infinite
loop, decorative rendering load or demonstrated exact-arithmetic fallback storm.
A small impact can repeatedly discover/solve an almost globally connected pile,
then dirty thousands of incident CCD queries. Fixed-iteration contact residuals
and erased support identity contribute to repeated contact rediscovery.

M91 retains the chronological first-touch path, actual-contact manifolds,
restitution/energy guard, ten velocity/four position iterations, physical materials,
60 Hz cadence, force/CCD quality and sleeping policy. It introduces no scene/name
checks, distance freezing, body removal, reduced cadence or physics-only demo flag.

**New numerical policy:** for free inelastic native-primitive events, separate
contact identity from invalidated impulse guesses. Retain zero-load anchors and
record actual TOI anchors with zero warm impulses. A support exemption survives a
trajectory change only if outward-rounded bounds certify the whole remaining
motion within the **existing** 5 mm position allowance (or smaller thin-feature
bound). No new pair or impulse comes from that certificate. Failed, uncertain or
unsupported proofs use ordinary CCD. See the [equations and scope](../../M91_COLLISION_CONTINUITY.md).

This deliberately changes some numerical trajectories; it is not a universal
bit-identical optimization. The criterion is a bounded persistent-contact model,
not zero-overlap physics. It is not a global penetration cap: ordinary solver
residuals in other pairs can exceed the position allowance.

**Compatibility boundary:** hard-jointed impact islands, external couplers,
elastic flights and events containing cooked/compound/displaced shapes retain
the original contact-cache/event policy. A geometric bound cannot certify the
reaction history of an arbitrary mechanism. The M77 bisection below established
that distinction, rather than hiding it with controller tuning.

**Exact derived-work improvements:** reuse unchanged conservative sweep supersets,
visit ordered joint adjacency only when an event needs it, publish the contact
cache only after its order changes (also on failure), and remove duplicate scalar
interval operations. The binary64 exponent fast test retains original outward
rounding and subnormal fallback. No public JudasJS API or gameplay code changed.
The lab script/UI changes are explanatory text only; all scenes, counts and
physical authoring remain byte-identical to the baseline.

M56 now records proof attempts/acceptances/partitions/fallbacks, coupled/elastic/
geometry fallback counts, peak island bodies/rows and inclusive cache/proof cost.
No per-contact event flood is introduced. Aggregate counters remain bounded.

## Matched performance

Milliseconds: **median / p95 / maximum**. Matrix rows are median-of-three run
statistics (including the median of each run maximum), not a pooled hard bound.
Dense baseline 155 row combines two retained runs (median medians/p95s, largest
observed maximum); final row is one final-source run, with preceding repeated
compatible stages retained. Consumer and articulation rows are single matched
runs, so small differences are not robust speedup claims. All raw distributions
remain available. No simultaneous owned benchmark/build/VM workloads were run.

| Workload | Baseline ms | Final-source ms |
|---|---:|---:|
'''+'\n'.join(rows)+f'''

The ball game's p95 fixed-step cost decreased by
**{100*(1-ap['fixed_ms']['p95']/bp['fixed_ms']['p95']):.1f}%** in this matched run.
Both versions pass the original 30 ball-mode checks; both coin versions pass all
31. Coin timings are effectively unchanged, including occasional >100 ms steps.
Counter/trajectory equality is **not** claimed for those games: the continuation
model changes contact evolution. No game input, body population, payout rule,
material, timestep or acceptance threshold was changed for the comparison.

The 15-case 64/256/1024 scaling matrix and 24 repeated controls pass all their
unchanged physical gates. Their trajectory digests match baseline. Most ordinary
steady workloads are similar; this is not a blanket speedup. Four imported rigs
retain the same contact-row/CCD totals, endpoint bounds and sleeping counts as
baseline. The kernel-only 2M-operation experiment is faster, but its percentage
must not be reported as whole-engine acceleration.

### Dense completion and unresolved tail

- Baseline requested 240 steps: **184/186 completed before separate 75 s kills**.
  Completed-step p95 was 2585.953/2462.017 ms, observed maxima
  3647.346/3515.550 ms. These are censored prefixes, not complete-run statistics;
  no duration is invented for the step interrupted by the timeout.
- Final-source 240 steps: **complete in {a['wall_seconds']:.2f} s**;
  p95 {a['fixed_ms']['p95_nearest_rank']:.3f} ms, max
  {a['fixed_ms']['maximum']:.3f} ms. Still far outside a 60 Hz budget.
- Final-source 270-step diagnostic (profiler **on**): max
  **{pr['fixed_ms']['maximum']:.3f} ms**. Peak event island: **973 bodies /
  2517 contact rows**, 51 impacts and 501599 TOI queries in that step. Approximately
  {pc["Physics impact island discovery ms"]/1000:.3f} s island discovery,
  {pc["Physics CCD search inclusive ms"]/1000:.3f} s CCD search,
  {pc["Physics CCD geometry inclusive ms"]/1000:.3f} s geometry and
  {pc["Physics impact solve ms"]/1000:.3f} s impact response. Exact counters
  are in `raw/profile-verified/after-270-r0/profile-shutdown.json.gz`.
  Categories overlap: **do not sum them**. Exact fallbacks are rare, unresolved
  geometry zero. Rendering is not the dominant fixed-step cost.
- A longer **post-compatibility, pre-conditional-publication** candidate requested
  600 steps and was killed at 90 s after **{long['completed_steps']} completed**;
  completed max **{long['fixed_ms']['maximum']:.3f} ms**. Its exact source and failure
  remain in `raw/closed-long/`. Conditional publication changes only redundant
  sorting/allocation, not physical arithmetic; prefix digests are checked against
  final source. That old timing is not relabeled as a completed final 600-step run.

The open mechanism is repeated global-component reconstruction and rescheduling
for successive impacts. A hard bound obtained by dropping contacts/CCD, freezing
bodies or capping physical events would violate this task. M91 makes practical
progress but does **not** establish a game-ready thousand-object dense-pile limit.

### Physical quality, memory and normal watch tour

The actual endpoint narrowphase observation finds {q0['deepest_endpoint_penetration_m']*1000:.3f} mm
deepest penetration in the baseline 155-step loose population, versus
{q1['deepest_endpoint_penetration_m']*1000:.3f} mm final. At 240 steps final is
{q2['deepest_endpoint_penetration_m']*1000:.3f} mm. All have 1024 observed loose bodies,
zero unresolved pairs/unsupported candidates. These are **endpoint** measurements,
not a maximum over the complete trajectory or an accuracy proof for every shape.

The final dense process peaks at {a['native_peak_rss_bytes']/1024**2:.1f} MiB native RSS;
this includes renderer/driver and is not VRAM. Baseline/other-run RSS is preserved
in the raw receipts; no memory reduction is claimed. Scoped owner-thread new
instrumentation restored baseline allocation counts: free-flight 1024/120 measured
steps **0 calls/0 bytes**; fully sleeping matrix **71280 calls / 15628800 requested
bytes** over 120 steps. Those existing allocations are not live/leaked bytes,
C allocator calls or worker-thread totals. Earlier eager-adjacency and unnecessary
stable-sort allocations are preserved as measured regressions and removed.

The normal 128-piece watch tour (256 dynamic / 389 total bodies, 96 joints),
1024x768 native hidden window, requested swap=1 and actual wall cadence, produced
{watch['frames']} frames / {watch['steps']} fixed steps in ~30 s. Fixed-step median/p95/max:
**{watch['fixed_ms']['median']:.3f} / {watch['fixed_ms']['p95_nearest_rank']:.3f} /
{watch['fixed_ms']['maximum']:.3f} ms**. Frame-interval median/p95/max:
{watch['frame_interval_ms']['median']:.3f} / {watch['frame_interval_ms']['p95_nearest_rank']:.3f} /
{watch['frame_interval_ms']['maximum']:.3f} ms; at most {watch['maximum_fixed_steps_per_frame']} fixed steps per frame.
This is not a physical-monitor/GPU certification or a 1024-pile FPS result.

## Correctness and preserved failures

- **{len(native['runs'])} final-source focused executables pass**: contact geometry,
  caches/lifetimes, SAT/Boolean/workspace, rigid motion/bounds, rigid/kinematic CCD,
  impact timing/solver/robustness/mesh cost, sleeping/parallel initial geometry,
  joints, CharacterMotor, M77 balance, imported ragdolls, origin shifts, wheels,
  powertrains, cavity coupling, mechanical attachments, sections, deformation,
  fracture. Impact robustness retains 4412 checks/zero failures.
- M91 interval reference: 300200 multiplies, 249998 divides, 50202 zero-divisor
  rejections, **zero endpoint-bit differences**. 406 continuation checks include
  arbitrary orientations, staggered anchors, large positions, represented-float
  negative control, unsupported/fault inputs, real momentum/energy/contact tests
  and short elastic departure/return. Sampled enclosure checks complement the
  derivation; they do not exhaust all floating-point motions.
- **Elastic regression preserved:** the unrestricted identity prototype suppressed
  a second bounce (0 rather than baseline 0.5625 m/s). Elastic flights now retain
  the original event/history policy; the baseline and corrected test pass.
- **M77 regression preserved:** unrestricted policy failed three of 119 unchanged
  checks. Separate no-certificate/no-history engine archives failed three/two;
  baseline passed. Restricting the new model to its demonstrated domain restores
  all 119 with byte-identical baseline output. Controller settings, fixtures,
  thresholds, timestep and solver budgets are unchanged.
- Rejected block-normal, identity-only, predictive-patch, alternate certificate,
  replayed-warm-load and adaptive-iteration experiments retain genuine failures,
  source snapshots and measurements. More iterations were not adopted.
- Late eager adjacency and unconditional cache publication added idle allocations.
  The final implementation builds adjacency only for a real event and stably
  publishes only a modified cache, preserving the ordering invariant on failure.
- Negative controls detect absent continuation, missing body contact and frozen
  mechanisms; the baseline short elastic case remains a positive control.
- M70/M77 cold writes/reads pass in separate processes; saved bytes are unchanged.
  Actual M77 restored and uninterrupted physical endpoints match and both satisfy
  original standing/contact thresholds. No arbitrary-pile bitwise cold-continuation
  guarantee or retroactive legacy-save compatibility is invented.
- Normal application: 51 unchanged checks pass, including sleep/wake, finite motors,
  impulses, spawning, scene changes, save/load and stale handles. Editor duplicate/
  Undo and Play/Stop restore identical authoring; frozen profiler inspection does
  not freeze simulation. Export and byte-identical relocated package pass all four
  registered scenes, pause/resume and reload without repository/CWD dependency.

See [final results](RESULTS.json), `raw/VERIFIED_RUNS.json`, final **verified** raw
receipts and [reproduction/provenance](REPRODUCTION.md). Old folders called
`final-*`/`closed-*` are explicitly developmental, not silently overwritten.

## Deliverable and one human checklist

Editable project: `projects/physics_stress_lab/physics_stress_lab.judasproj`.
Standalone: `.cache/m91/verified-delivery/package`; relocated runnable copy:
`/tmp/judas-m91-verified-moved/judas`. [Project controls](../../../projects/physics_stress_lab/README.md).

1. Open the normal lab and press **F5**; watch shelves/hinged paddles contact balls.
2. Use **M**, wait for natural sleep, then **J/K** to wake one/all mechanisms.
3. Inspect collisions and piles with **1/2**, then deliberately try **3**; report
   the known long dense-pile stalls rather than treating a brief smooth start as closure.
4. Use **P** to add real colliding bodies; inspect for tunnelling/explosive motion.
5. Try **4** and verify gravity/contacts rotate coherently.
6. Save/load with **F6/F7** while mechanisms move; pause, reload and editor Stop.
7. Repeat the relocated standalone from outside the repository. Optionally revisit
   both Tuppence modes; no game-side performance workaround was introduced.

## Limits and checkpoint assessment

Validated improvements are eligible for an Experimental checkpoint. **Human
acceptance is pending; main remains untouched.** Severe late dense-island stalls,
coin-game tails, generic cold-pile continuation and existing approximate CCD limits
remain open. Per-pair interval work is bounded; whole-step work is not globally
bounded by that fact. Windows/Android and broader workloads are not inferred from
these Linux samples. FLU-001 and independent fluid research remain deferred.

No protected history, accepted fluid implementation, unrelated game content or
historical evidence was modified. Exact changed paths are in `CHANGED_FILES.txt`;
new raw evidence is append-only to this milestone. No next milestone was started.
'''
# Correct relative project link from evidence/m91 to repository projects.
text=text.replace('../../../projects/physics_stress_lab/README.md','../../../projects/physics_stress_lab/README.md')
(d/'REPORT.md').write_text(text)
print('report written',len(text),'bytes')
