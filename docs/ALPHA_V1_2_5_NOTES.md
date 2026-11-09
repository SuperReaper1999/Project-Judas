# Judas Engine Alpha v1.2.5

Machine version **1.2.5-alpha.1**. This patch includes the full committed
Experimental history following Alpha v1.2.4.

- The SDK ships an editable terrain and navigation workshop and a reference
  with exact scene keys for cooking terrain, baking a navmesh and moving NPCs.
  See [SDK authoring](SDK_AUTHORING.md).
- Scripts can edit navigation obstacles and links and tune agent corner
  tolerance. Oversized blockers are rejected before exceeding the tile
  carving limit. See [navigation](judasjs/navigation.md).
- Runtime tilemap edits update collision and persist through modern save
  slots. See [tilemaps](judasjs/tilemaps-2d.md).
- JudasJS adds bounded multi-hit shape overlaps, centre-of-mass queries,
  off-centre force and collision filtering controls. See [physics](judasjs/physics.md).
- Scripts gain emitter audio, Basic Water, particle and secondary camera
  controls. Camera targets can resize at runtime and retain the last working
  allocation if a resize fails. Camera presentation and particle settings
  support readback. See [effects and cameras](judasjs/effects-camera.md).
- The live JudasJS API inventory probe now constructs AI classes with valid
  arguments and releases the handles it creates.

Advanced Fluid Dynamics remains locked in Alpha. Basic Water and swimming
remain available. See the [support matrix](ALPHA_SUPPORT_MATRIX.md) and
[previous patch (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/ALPHA_V1_2_4_NOTES.md).
