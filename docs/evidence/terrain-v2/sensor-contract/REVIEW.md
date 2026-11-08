# Terrain sensor preflight correction

A terrain owner uses an ordinary static Box as its logical seed and replaces that
seed's support with generated triangle colliders. Native placement and runtime
creation previously accepted `body.sensor=true`, but MK2 rejects TriangleMesh
volume sensors. Terrain synchronization ignored that setter failure, leaving
blocking triangle surfaces behind a nominal sensor definition. This was an
integration defect, not evidence that triangle-volume sensors were supported.

The new native-placement and runtime-create controls both fail on397b2467; their
raw failures are retained. Native placement and runtime submission now reject the
combination before any terrain publication, with an actionable instruction to
use a separate box, sphere or capsule trigger. The original sensor body, sensor
flag and body count remain unchanged after a rejected runtime request.

After correction: native component39 and world96 checks pass, retaining all prior
physical assertions including300 supported seam steps and exact monolithic
trajectory comparison. Geographic component40write/9cold-read and actual new-project
editor46 checks pass (both desktop and portable graphics). A relocated standalone
with material detail and reliable particles excavates, traverses, saves and
cold-restores successfully. See raw commands/results. No test tolerance or physical
behaviour is changed. No triangle-sensor emulation, release change, fluid unlock,
installed SDK update or human acceptance is claimed.

This deliberately changes an incorrectly accepted configuration into an explicit
failure. Supported ordinary sensors and non-sensor terrain remain unchanged.
