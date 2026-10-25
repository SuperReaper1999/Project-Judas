# Folder-picker navigation follow-up — 2026-10-25

Operator feedback: reaching a subfolder required choosing it, leaving the picker,
and opening Browse again. Expected ordinary file-manager navigation.

Investigation against checkpoint `8a1516beda5ad07e7c429ec84e0eb5f051977267`:
double-click navigation already worked in an isolated ImGui pointer test. The
initial suspicion that a selectable automatically dismissed this child listing
was not reproduced and is not claimed as the root cause. Single-click selected
a name and Choose confirmed it; there was no explicit action to enter that
selection and no navigation hint. This was a discoverability/workflow deficiency.

The picker now provides Open folder for a selected directory, a double-click
hint, and Enter in the location field. Navigation shares one checked path;
Choose remains explicit confirmation. Selectables also explicitly suppress popup
auto-close. No filesystem creation or project-creation semantics changed.

Validation: Release editor and picker test target built successfully. Fifteen
focused checks passed, including actual pointer double-click and nested
Open-folder/Parent navigation. Running the same test against the original source
fails the new Open-folder navigation check; see the two raw logs here. The first
test draft only checked the returned child path, which could pass without
navigation; the retained test uses Parent to distinguish those behaviours.
The corrected editor was launched for operator review without closing the
existing editor session. Human acceptance of this follow-up remains pending.

Commands from repository root:

```sh
cmake --build build --target judas_editor_file_browser_tests judas_editor -j 2
build/judas_editor_file_browser_tests
```

Source SHA-256:

```text
5bb3d072260ef35bfbceae49b8a64c40a905b2539b028c2ae31c9d4333096d23  src/editor/FileBrowser.cpp
9580f92fbb18c984a7925da33f15fed25599331212f0febc6ee584e68c638ee2  tests/EditorFileBrowserTests.cpp
ebc9f8b0347b72fc7ccab64216bc987f4afccbf70f004f216e6c4b00b2d3b968  docs/EDITOR_GUIDE.md
```
