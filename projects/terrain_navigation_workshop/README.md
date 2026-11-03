# Terrain Navigation Workshop

A small SDK-only example. A scripted orange courier crosses a 32 m yard on
cooked terrain collision. A hill off the route shows the sculpted heightfield.
The starter scene contains empty asset references until prepared.

Copy this whole folder out of the SDK to a writable directory. With Python 3,
run `python3 prepare.py /absolute/path/to/SDK` from the copy. This creates the
terrain source, sculpts and cooks it, fills its scene asset references, and
bakes the navmesh using the SDK tools. Run preparation once per fresh copy.

Open Workshop.judasproj with the SDK editor, preview the baked polygons, and
press Play. The courier travels from X=-8 to X=8 and stops. Its script prints
TERRAIN_NAV_COURIER_ARRIVED once the agent reports arrival and the actual motor
position is within 0.5 m of the destination horizontally at a standing height.

See the SDK's docs/SDK_AUTHORING.md for the raw fields, units and manual recipe.
The scene and script are ordinary editable project files after preparation.
