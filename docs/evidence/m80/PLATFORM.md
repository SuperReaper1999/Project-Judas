# Platform scope

Native Linux is the M80 acceptance platform. Release focused tests, normal
application/resource/save/streaming lifecycles, editor authoring/Play-Stop,
standalone and moved-package checks pass as recorded in RESULTS.md. Rendering
uses SDL offscreen for automation; that backend does not implement relative
mouse capture. The operator accepted the Linux desktop M80 demonstration on
2026-10-13; see [the acceptance record](HUMAN_VALIDATION.md). This is not a
claim of individual pointer/device certification or Windows acceptance.

The existing `win10` VM was checked on 2026-10-13 and is shut off. No native
Windows M80 build or behavioural result is claimed. Windows M80 validation
remains outstanding, informational and nonblocking. Physical Windows GPU
performance/visual acceptance is also unmeasured. No VM configuration changed.
