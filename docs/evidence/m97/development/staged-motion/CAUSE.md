# Preserved private-staging motion failure

The moved standalone's fast, fixed-clock tour left quarry0, returned while its
terrain job rebuilt asynchronously, and observed ordinary returned stock falling
below its resident floor. Trace frame 650 records the first crate at approximately
Y=-39.57m, despite its captured resting state at Y=0.50m. Native normal-window
application checks did not reproduce it because they yielded enough wall time for
preparation to finish in few simulation steps.

Staging disabled collision but did not suspend dynamics. During the longer private
window the actors accumulated gravity and integrated through the floor before
atomic publication. The correction adds a separate internal simulation-suspension
flag for private staged bodies. Gravity/forces/impulses and motion integration do
not advance them, while ordinary collider-disabled gameplay actors retain their
existing motion semantics. Publication resumes exact captured motion; it does not
insert settling steps or reset a visible actor. The final standalone follow-up
checks returned stock heights before and after cold load in addition to exit code.
