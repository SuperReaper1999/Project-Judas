# Documentation alignment follow-up

Current reference checkpoint: accepted M69, 8 October 2026.

We need a better repeatable way to keep documentation aligned as Judas changes.
The October audit found accepted milestones still labelled pending and older
limits conflicting with current references. A dedicated maintenance improvement
is deferred; this refresh does not introduce generators, CI or runtime changes.

For future checkpoints:

- Update README and Architecture current status/capability summaries together.
- Update the affected subsystem reference, JudasJS declarations, inventory and
  coverage manifest when public contracts change; use the existing drift checker.
- Record acceptance separately from original candidate evidence. Label superseded
  guidance historical and link its current replacement; preserve failures/results.
- Check old limitations and linked examples when a feature supersedes them.
- Keep the local gitignored PPM current, but never silently turn it into a tracked file.
- Preserve M67's deferred authoring review and distinguish automated controller
  evidence from hardware feel testing.

Useful next improvement: a small checkpoint checklist/status index and targeted
checks for stale status labels and local links. API coverage alone cannot detect
incorrect prose, ownership claims or acceptance status; these still need review.
No future milestone number or implementation is assigned here.
