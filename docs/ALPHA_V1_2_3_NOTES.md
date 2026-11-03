# Judas Engine Alpha v1.2.3

Machine version **1.2.3-alpha.1**. This patch is prepared for Alpha
qualification and operator review.

- **Editor** gains File → Build & Export and a model import route from the
  ordinary asset picker. Large hierarchies reuse filtered rows and draw the
  visible portion of the list. Play checks each distinct model archive once
  per request, including fresh terrain validation.
- **Rendering** reuses presented transforms and skin palettes across cameras
  and shadow passes. Visibility and LOD remain per-view.
- **Resource management** avoids duplicate idle accounting and orders bulk
  eviction candidates once while retaining pinning and camera demand.

The [editor measurements](evidence/editor-first-pass-20261103/README.md),
[rendering measurements](evidence/render-pose-reuse-20261103/README.md) and
[resource measurements](evidence/resource-pump-20261103/README.md) record the
Experimental development fixtures. They include software rendering and
headless bookkeeping controls. Hardware timing and first-frame stalls need
separate validation.

Advanced Fluids remain locked in Alpha. The separate fluid research and other
Experimental changes are outside this patch. For the existing platform scope
and release procedure see the [support matrix](ALPHA_SUPPORT_MATRIX.md)
and [release record](RELEASING.md).
