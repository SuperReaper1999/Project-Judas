# Runnable review packages and prioritized human checks

**Candidate is not fully passing.** REG-001 is an explicit integration blocker; PHY-002 is partial and FLU-001 is deferred. No acceptance or commit is requested by these instructions.

## Linux packages

From the repository root:

```sh
.cache/m78-followup/review-balance-package/judas .cache/m78-followup/review-balance-package/game.judasproj
```

Fresh Balance Lab export: normal runtime, four scenes, seven assets. This is a review/reproduction package, not a passing corrective-step certificate.

Portable shared-terrain/mod review, with its own data/config store and both exact packages:

```sh
/tmp/judas-m78-followup-terrain-review/run-review.sh
```

Also retained under `.cache/m78-followup/review-shared-terrain-package/`. Launcher uses ordinary `judas_mod` install/enable then ordinary `judas`; no hidden engine mode. It explicitly trusts only the known hill-map executable payload. `--prepare-only` performs store setup without launching. Keys **1** replacement landscape, **2** contributed hill map, **3** hub, **4** request/release replacement region; WASD/Space, mouse, Escape, R, F6/F7 are existing authored controls.

Owned authoring projects: `.cache/m78-followup/dependency-final-project/landscape_fork.judasproj` (shared terrain, both refreshed scenes); `.cache/m78-followup/independent-fork-project/landscape_fork.judasproj` (independent fork proof). Original project/kit copies were not edited. Editor:

```sh
.cache/m78-followup/release/judas_editor .cache/m78-followup/dependency-final-project/landscape_fork.judasproj
```

Review only owned copies. Save/cook terrain, inspect dependent saved navigation, explicitly rebuild. Dirty scene drafts must be saved first. Private unpublished base scenes cannot be discovered; published read-only scenes require an owned fork/replacement. Failure/cancellation preserves last-good outputs; existing immutable bake files are retained.

## Human checklist

1. **Confirm known REG-001:** Balance F1, Stride selected, P, wait, B, wait for standing, J (29 N s). First step should lift/land; current candidate can then fall during the next step. Record rather than accept it as recovered standing.
2. Check independent standing/support/recovery controls: F2 moving/rotated support; F3 weighted Anchor/Receiver P then G; F4 radial. No foot pinning/root teleport or forced sleep.
3. In the shared terrain package inspect keys 1 and 4, real edited collision and both scene/region replacements; pause/reload/save/load. Compare owned authoring scene and moved package.
4. In the owned editor inspect refresh/rebuild rows for **both** shared saved scenes after a small sculpt/cook. Deliberately leave one stale: packaging must reject it. Rebuild both: normal scene validation should accept. Appearance-only changes should retain freshness.
5. Review the independent terrain fork without altering the original/published kit. Test cancellation/failure only on another disposable owned copy.
6. Inspect meshes/impacts and normal sleep at ordinary cadence; do not equate a smoother visual impression with the still-failed four-rig p95 numerical gate.
7. Review fluid contract proposal separately. Original check/physics are unchanged; no claim that it now passes.

Fresh Windows editor/package/hardware testing remains outstanding. Existing guest review package is preserved, but is not mislabeled as the new combined candidate. Automated native checks cover the changed numerical/service paths; don't use Linux package binaries on Windows.
