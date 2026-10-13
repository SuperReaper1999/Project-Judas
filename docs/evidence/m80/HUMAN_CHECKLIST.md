# M80 human checklist — pending

Open `projects/bending_lab/bending_lab.judasproj` in the editor and Play, or run
the normal exported `judas` in `/tmp/judas-m80-bending-lab-20261013`.
WASD/mouse/Space move, look and launch; Escape pauses.

1. **Elastic return:** press **B**, watch green A bend, then press B again.
   It returns toward its gravity-loaded shape rather than snapping to rest.
2. **Permanent deformation:** orange B receives the same load. After unloading,
   compare its retained shape with A; the physical material should stay bent.
3. **Physical failure:** press **H** through increasing load on blue C. Watch
   connected material bend then actually separate/fall. Reload with **F9**, change
   an interface strength or support in the editor, and compare the outcome.
4. **Deformed contact:** the orange physical press dents purple D. Use **G** to
   pick it up/drop it and **T** to throw; verify it meets the current dented
   surface. Click a bent surface to apply a hit-point impulse.
5. **Arbitrary gravity:** inspect cyan E, then **2** for the actual radial world.
   Repeat loads/contact. **1** returns to uniform gravity.
6. **Independent instances:** **P** alternates elastic/plastic prefabs. Check
   their geometry/state does not overwrite another instance.
7. **Persistence/lifecycle:** **F5/F8** saves/loads deformed and failed state;
   **F3** requests/releases the streamed annex. Check revisit/reload and editor
   Stop leave no stale state or unintended authored changes.
8. **Standalone:** repeat core interactions, pause/resume, saves and scene change
   in the exported package; move its directory and launch from elsewhere.

Automated input, screenshots and numerical tests do not mark these human checks
complete. Windows native M80 checks remain informational and outstanding.
