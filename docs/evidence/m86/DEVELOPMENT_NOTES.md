# M86 development observations

These are actual development receipts, not additional acceptance claims. Final
results belong in `REPORT.md` and `final/`. No earlier milestone evidence changed.

1. Initial C++ compilation caught mixed GLM scalar types and a missing test-only
   experimental-header definition. The original build logs are retained.
2. The first real QuickJS test caught a new binding error: `KnownOptions` returns
   a Boolean, but the weather caller returned `JS_EXCEPTION` without installing
   an exception. Unknown profile keys now throw a real `TypeError`. Validation
   remains strict; no requested value is silently substituted.
3. Two fixture assumptions were wrong: M59 preload status is `prepared`, and the
   legacy default `interact` binding is G, not E. The new tests now inspect the
   actual status and explicitly author their logical input map. Original failures
   remain in `development/`; these were not engine defects.
4. Before first GL execution, review corrected cloud advection continuity, the
   common periodic noise reduction, stable spherical intersection arithmetic,
   homogeneous far-ray construction and the funnel/debris rotation convention.
5. The initial GL checks passed 35 numerical/image assertions, but visual review
   found weak funnel/bolt presentation. A finite-cylinder, radius-aware funnel
   march and proper ray/segment lightning distance replace the inadequate bounds.
   Screenshots and stdout from the earlier implementation are retained.
6. The actual application run then exposed severe 4,000 km orbital cloud aliasing
   that its smaller 12 km render fixture had not exercised. The exact screenshot
   is retained. Mip-filtered noise and a distance-dependent cloud representation
   corrected unresolved detail, but did not remove the horizontal bands in the
   actual scene. Readback and a distant tilted-plane negative control isolated
   a separate depth-ray mismatch: nearest full-resolution depth was reconstructed
   along the neighbouring quarter-resolution cloud ray. Explicit interpolation
   of smooth projected depth, with conservative silhouette rejection, corrected
   it. The unchanged oblique-occluder image disagreement fell from 97.209278 to
   0.280691 mean byte values; the actual 4,000 km application view was rechecked.
7. The initial application run reported 66 checks / 3 failures. Thunder's PCM was
   authored at 22,050 Hz, while the existing `AudioSystem::CreateClip` requires
   48,000 Hz. Weather now supplies the existing mixer contract directly (three
   seconds of original mono PCM); no alternate audio backend or resampler exists.
8. The other two application failures queried `ScriptSystem::Capture` before
   pending restored records had become live VM instances. The test now explicitly
   observes normal `Synchronize` construction, then requires exact archived JSON
   before any callback or clock advance. No engine save behaviour or tolerance
   changed. Native weather and physical endpoints already matched in the failed
   runs; the cold-save assertions remain exact at the correct boundary.
9. Additional lifecycle adversarial checks caught invalid weather-owner transforms
   being published before validation, and authored reset rebuilding a bounded
   field from its old moved pose. Transform preflight now rejects invalid spatial
   state atomically; reset rebuilds after the authored physical poses are restored,
   including unscripted weather-only owners. The original three failures and the
   corrected 50-check world receipt are retained.
10. Final review found the storage envelope still capped at 19 participants while
    the supported composed-world schema permits 21. The original 36-check fixture
    failed four 20/21-participant encode/decode cases. Matching that bounded
    storage limit to 21 fixes composed weather saves; 22 participants still
    reject, and decoder rejection preserves the caller's output.
11. The first normal export rejected the lab's scene inclusion list: explicit
    `exportScenes` replaces the automatic scene set, and the list omitted the
    startup flat scene. The generator and current project now include all three
    registered scenes. The exporter itself and its rejection policy are unchanged;
    the corrected package was moved and exercised through the shipping runtime.

Cloud/noise GPU quality never feeds the wind, forces, seeded strike schedule or
physical continuation. Surface appearance and GPU precipitation are explicitly
approximate. Final performance receipts must be taken without concurrent builds.
