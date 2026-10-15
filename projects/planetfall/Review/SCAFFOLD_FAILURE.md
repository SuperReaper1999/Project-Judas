# Preserved access-route failure

Native coastal scene, 60 Hz authored input, radius .3 m capsule, .6 m half-height, .35 m step height. First attempt used .2 m risers/.3 m treads; wider .6 m treads also failed. Both original logs are retained.

At the blocked first lip, a flat probe returned dot(normal, up)=.2527; the raised sweep cleared, and a down probe farther onto the step found dot=.8729. The normal support query encounters the non-supporting rounded side contact before the floor. This is an existing motor/support limitation exposed by game content, not a proven new regression. No engine code or solver threshold was changed.

The game now has an actual oriented physical service ramp and roof landing. The old stair geometry remains as authored scenery/collision. The subsequent access test exercises the ramp and actual recorder interaction; it does not retroactively pass the failed automatic-stair contract.
