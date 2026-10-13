# Reproduce the M83 follow-up

From repository root, Release Linux:

```sh
cmake --build build -j2 --target judas judas_editor judas_export \
  judas_collision_geometry_tests judas_impact_robustness_tests \
  judas_mechanical_crash_application_tests judas_mechanical_damage_application_tests
build/judas_collision_geometry_tests
build/judas_impact_robustness_tests
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/judas-m83-repro-user \
  build/judas_mechanical_crash_application_tests projects/damage_lab/Scenes/crash.judas \
  rest /tmp/judas-m83-rest
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/judas-m83-repro-user \
  build/judas_mechanical_crash_application_tests projects/damage_lab/Scenes/crash.judas \
  drive /tmp/judas-m83-drive
```

The app fixture controls only ordinary logical W. It records actual solved bodies,
contacts, mounting reactions and mechanical continuity. `rigid-drive` is the
isolated ordinary control mode; `development/rigid-control` preserves its scene
and generator. Other development subdirectories preserve exact failed content
and authoring controls; their /tmp paths can be replaced with fresh scratch paths.
They are intentionally failed comparisons, not current acceptance fixtures.

`tools/CreateMechanicalDamageLab.py --crash-only` regenerates only the new crash
cook, scene and prefab. It leaves controller/UI/project and old fixtures untouched.
Use a scratch copy if independently comparing generation hashes.

Original M83 application modes remain `write`, `read`, `lifecycle`, `oblique`;
read is a separate fresh process sharing a scratch XDG_DATA_HOME. The fixture
explicitly selects original Scenes/flat.judas from a project argument.

Export with `build/judas_export PROJECT FRESH_OUTPUT build/judas`; move the package
and launch its executable from /tmp. `final-export-smoke.txt` is the exact executed
logical-control harness; change screenshot paths for a fresh evidence destination.
Editor smoke operates only on a copied project, with the root powertrain reordered
first for the existing inspector automation. Do not run it on the canonical project.

Do not overwrite original milestone evidence or claim Windows validation from
these Linux receipts. Public APIs and physics cadence are unchanged.
