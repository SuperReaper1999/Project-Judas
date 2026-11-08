# Material checkpoint SDK qualification

Exact engine source: `b5c1bc0dc5126aa2ecc3129a702efdabe8d7466c` on Experimental.
Local package: `.cache/terrain-v2/sdk-native-preview-b5c1bc0d`.
All 17 tools probe successfully; the 842-file package verifies before and after
consumer testing. Standard SDK project creation/editor/export workflow passes all
8 operations. Both SDK-contained, material-enabled terrain Workshops cook, export,
relocate, excavate, traverse and cold-restore. Their normal texture is present in
the exported game, and the flat game restores its reliable particle impact state.
See `RESULTS.json`, per-game records and raw logs.

This SDK includes terrain material detail. It predates the following structured
collision optimization: do not claim that later engine change is packaged here.
It is a local GNU15.2 / GLIBC2.43 build, not an Ubuntu24.04 distribution, published
release or new Windows/Android/browser qualification. Build provenance honestly
records the unrelated existing untracked-source fingerprint. The installed RPG
SDK selection is unchanged. No human acceptance is inferred.
