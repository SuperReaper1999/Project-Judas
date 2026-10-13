# Cold observation boundary

The first application assertion combined successful whole-world capture with
exact deformable-byte equality. It ran at publication, before the restored
script lifecycle callbacks had initialized. Capture correctly rejected the
request: `required script instance not initialized: entity 10 slot 1; retry
after lifecycle boundary`. This was a test boundary error, not lost geometry.

The corrected check freezes elapsed time, completes the restored lifecycle
boundary, asserts zero resumed fixed steps, then compares the complete native
participant bytes exactly. Physical motion is subsequently resumed. No save
implementation was changed to bypass script initialization.
