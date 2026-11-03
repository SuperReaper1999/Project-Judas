# Actual Hierarchy panel CPU and input controls

This Linux Release diagnostic re-links the already built CMake editor objects,
substituting only the tiny ImGui harness for EditorMain. It calls the actual
DrawHierarchyPanel with no graphics backend, viewport render, engine workers or
engine startup. The scope is CPU submission of the Hierarchy panel. Before and
after run serially with the same deterministic scene, hierarchy, project tags,
window dimensions and font scale. The two row positions have nine steady draws
each. Nearest-rank p95 therefore equals worst. The first draw is tracked separately
because it constructs the filtered list and includes ImGui cold layout.

At 8,192 objects, 100% scale and top of the list, unfiltered steady median/p95
went from 203.420/203.781 ms to 0.006462/0.006543 ms. With a named tag filter it
went from 32.176/34.508 ms to 0.006672/0.006753 ms. These are panel times for a
deliberately deep, synthetic authored scene. They do not measure editor frame rate,
fresh query lag or startup. The interactive contexts disable clipping during inline
rename and open menus to keep focus and popup owners alive.

The single first-draw measurements at that size and scale were 432.95→0.60 ms
unfiltered and 58.40→23.39 ms tagged. A new search still needs to examine authored
properties for objects that do not match directly. The cached-frame results do
not represent typing latency.

The actual mouse and keyboard control exercises the bottom row of a 512-item
list. It verifies scroll extent, range selection, context popup, exact child
target, F2, typed rename and Undo. It passes under stock ImGui and Judas dark/light
preferences, at both 100% and 150% scale. The first attempt sent F2 while the
dismissed menu's search still set WantTextInput. The follow-up waited for focus to
settle and reproduced a genuine 150% scrolling error before the row-stride fix.
The same 512 rows had ScrollMax 13,328 at the top and 13,840 at the bottom.
Integer row stride keeps the final default-theme 150% maximum at 13,328.

Build the ordinary Judas editor in each corresponding source checkout with the
GNU Make CMake generator. Run the following from that checkout root. All outputs
should go to a fresh directory. The helper reads the existing editor target's
compiler and link flags. It does not compile any engine source.

```bash
out=$(mktemp -d /tmp/judas-hierarchy.XXXXXX)
e=/absolute/path/to/this/evidence
python3 "$e/link-benchmark.py" path/to/configured/build src/editor "$out/hierarchy"
"$out/hierarchy" 9
python3 "$e/link-benchmark.py" path/to/configured/build src/editor \
  "$out/interaction" "$e/HierarchyInteraction.cpp"
"$out/interaction"
```

The selected baseline was the clean Experimental checkpoint
84b94833df172c9962c61bff6e5a9d8597c95cec. The two verified baseline object SHA-256
values were `7e202018daf0611ab2f666deb96fe3806714e84a4b0a2d79f8ae37a7d8d35acb`
for EditorPanels and `8ce6db6573ff24f43ca88ed9a02f0a6d5d78f0783eac5623e76d5611fa4add29`
for EditorHistory. The candidate source fingerprints are in PROVENANCE.txt.
Binaries remain local.
