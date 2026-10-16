# M89 operator acceptance

On 2026-10-16 the operator tested the Core Scramble desktop package from
`945b76390f50d15fe2297664fa23ec349b7cd85d` and reported:

> works well push to main <3

This accepts M89 for release promotion. No additional individual checklist items,
physical controllers, Windows execution or Android hardware testing are inferred.
Those platform/device gaps and the existing limitations remain recorded in the
candidate report. M88 phone acceptance does not certify M89 phone behaviour.

Before promotion, all recorded `final-source.sha256` checks passed against the
unchanged tested candidate, and both remote branches matched their local refs.
The promotion adds only this acceptance record and updates the acceptance wording
in `docs/M89_LOCAL_MULTIPLAYER.md` and `REPORT.md`. Implementation, game content,
packages and validation results are unchanged. Original candidate fingerprints
remain preserved; the two documentation edits are intentional post-review changes.

Main is fast-forwarded through the exact tested implementation and this
documentation-only acceptance checkpoint. No validation reruns or tags are needed.
