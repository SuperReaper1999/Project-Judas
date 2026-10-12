# Preserved focused-fixture inspector failure

The first new focused-driver run against the unchanged checkpoint archive
reported **41 checks / 4 failures**. This run is retained here with its exact
compiled source, receipts, logs and captured sleep participants.

Its `SleepIds` byte inspector incorrectly declared `deferredAcceleration` as
`glm::vec3`. The real `PhysicsWorld::Impl::BodyRecord` field is `bool`, and
`PhysicsWorld::PersistSleep` serializes that boolean after `lastAcceleration`.
The wrong inspector consumed excess bytes, misaligned later rows and raised
`nonfinite saved number` / `truncated save field`. The captures themselves
succeeded. This is a new test-observer defect, not an engine sleep defect.

The inspector correction changes only its local decoded variable to `bool`.
The production serializer, malformed-payload builder (which calls the real
public `PersistSleep`), restoration guard and existing fixtures are unchanged.

This flawed first run did not reach the excluded actor's full restore check;
it must not substitute for the preserved original 22-check / 1-failure proof
or be reported as an engine before/after result. Corrected reruns use fresh
outputs and separate receipts.
