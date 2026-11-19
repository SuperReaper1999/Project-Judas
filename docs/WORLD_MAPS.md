# Baked world maps

A world map associates an ordinary PNG texture with an authored horizontal X/Z region. Runtime UI samples the texture directly. A HUD crop and a full map can share the same resident asset. A map view can display engine-baked imagery or hand-painted artwork with matching bounds and orientation.

## Bake and register an image

Save the desired scene in the editor. Select the intended ground region using the scene's world coordinates and choose an elevation interval that encloses its visible surfaces. Use the SDK scene author:

```sh
"$SDK/judas_scene_author" --bake-map MyGame.judasproj Scenes/market.judas \
  Assets/Maps/market.png -16 -16 16 16 -10 60 1024 1024 0
"$SDK/judas_scene_author" --track-asset MyGame.judasproj Assets/Maps/market.png
```

The arguments after the path are minimum X, minimum Z, maximum X, maximum Z, bottom Y, top Y, pixel width, pixel height and orientation. A scene argument of `-` uses the project's startup scene. Coordinates are **absolute world coordinates**. Resolution is 1–4096 pixels on each axis, within the host's render-target limits. Choose the aspect ratio of the oriented region to preserve geography without stretching.

Orientation specifies the edge at the top of the image.

| Orientation | Top points toward |
|---|---|
| 0 | North (−Z) |
| 1 | East (+X) |
| 2 | South (+Z) |
| 3 | West (−X) |

An optional final layer mask uses the same bits as the renderer. For example, `0x5` includes layers 0 and 2. The default includes all layers. The baker uses the shared scene observer and saves its resolved display colour. It reads the authored scene, including its initial terrain and objects. It does not load a gameplay save, run gameplay callbacks, or use a player's camera. Maps that depend on content assembled by game scripts need an authored bake scene or supplied artwork. A single scene and planar region define this increment.

The tool atomically replaces the named PNG on success. Rebake to the same path to retain its asset ID and sidecar, then rescan the editor's assets and rebuild the export. For authored artwork, import the PNG in the editor or track it with the SDK, and use its matching world bounds. Keep a record of those settings with the scene and script.

## Display the map and its markers

Add an **Image** to a normal runtime UI document and set its texture to the registered asset. Assign the same texture ID on a second image for a full map or an inset. Author a marker as a small child element of its map image, using free layout, no image padding and zero marker offset. Enable rectangle clipping on the parent for markers near its edges.

```js
import {WorldMap, world} from 'judas';

const map = new WorldMap(document.get('minimap'), {
    min: {x: -16, z: -16}, max: {x: 16, z: 16}, orientation: 0
});
map.setView({
    center: world.toAbsolute(player.transform.position),
    width: 16, height: 16, zoom: 1,
    heading: 0, circular: true
});
map.placeMarker(document.get('player_marker'), world.toAbsolute(player.transform.position));
```

`setView()` with no options shows the complete image in its authored orientation. A centre and width/height choose a subsection in world units. Zoom divides those spans. Heading is in radians clockwise from north, so use zero for north-up or the player's heading for player-up. The image rectangle defines the viewport. Use a square element for a circular minimap, or the selected span's aspect ratio for undistorted scale.

`worldToMap({x,z})` returns top-left normalized coordinates and a `visible` flag based on region and mask. `mapToWorld({x,y})` returns an X/Z point, including for coordinates outside the visible area. Use it to convert a pointer hit or place a waypoint. `placeMarker` positions an existing UI child and hides its centre outside the visible area. Circular masking applies to map pixels. Use small icons or provide your own edge handling for icons that straddle the circle. For facing, convert the player's position and a point ahead using the same view, then use their difference to position your facing cue. `detach()` clears the map sampling transform. An image can also be switched to a live render-camera preview through the existing API.

Use the [example script](judasjs/examples/world-map.js) and [UI recipe](judasjs/examples/world-map-ui.json) together. Bake the example bounds shown above and pass `9212a745649a43a2b86573421948eaa0` as the last argument to `--track-asset` in a fresh project. Use `judas_scene_author --structured world-map-ui.json Assets/Maps/maps.judasui` and track that UI and the example script. Attach the script to the player and assign the UI asset to `mapUi`. Its large and small map images share the same texture. Include the UI asset and its script in the project's export closure.

Recreate map views and reacquire UI handles from a script’s `restore()` callback when loading a save. Map pixels remain an asset. Persist discovered locations, custom markers and waypoints through ordinary game save state, using absolute coordinates and a game-defined region ID. Present player-built structures as overlays or update an authored bake separately. This increment uses one texture per region. Games can use several region descriptors, and future tiled or multi-resolution imagery can use the same world plane, orientation and normalized view coordinates.
