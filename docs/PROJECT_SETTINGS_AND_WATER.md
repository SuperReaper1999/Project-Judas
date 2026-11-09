# Project Settings and water

Open **File → Project Settings**. Save project settings separately from the scene. Stop and restart Play after policy changes; controls are locked during Play. Unsaved project changes participate in the normal Save/Discard/Cancel workflow.

| Category | Configuration ownership |
| --- | --- |
| General | Name, startup scene, icon |
| Rendering | Visual resource budget; scene lighting/exposure remain scene-owned |
| Physics and gravity | Tags/collision registry; gravity remains authored or scripted |
| Input | Logical actions, axes, vectors and bindings |
| Audio | Groups, gain, mute and pause |
| World and persistence | World manifest, save identity and evolution policy |
| Navigation | Profiles and areas |
| Localization | Locale, catalogs and fonts |
| Water | Independent Basic Water and swimming switches |
| Networking | Explicit game-owned session policy; no automatic listener |
| Experimental | Advanced Fluid Dynamics opt-in, warning and build availability |
| Export | Scene policy, dependency closure and destination |

## Basic Water

`basic-water "true"` and `water-swimming "true"` are defaults. Create a Basic Water component from Create or the Inspector. Configure bounds, colour, waves and current. This is prescribed visual/swimming water: no conserved flow, pressure solver or particle solver is allocated. Disabling swimming leaves the surface visible.

Try [Stillwater](../projects/basic_water_lab/README.md). Its project-owned controls provide movement, pause, Space to swim up and Q to dive.

## Advanced Fluids V2

`advanced-fluid-dynamics "false"` is the default. Experimental builds require explicit confirmation of the performance/limitations warning. Alpha builds enforce a compiled lock: authored opt-in cannot bypass it through a scene, save, script, environment variable or export. Disable the setting to use Basic Water. There is no conversion of advanced saves into Basic Water.

The sole advanced backend is [Game Water V2](ADVANCED_FLUIDS_V2.md): a coarse conservative physical grid, finite air reservoirs, MK2 body coupling and a separately reconstructed visual surface. The fluid volume component owns grid dimensions, cell size and initial fill. Backend selectors, PBF emitters and M54/M55 liquid bakes no longer exist.

With advanced simulation disabled, fluid component metadata remains editable but allocates no solver, contributes no advanced force and creates no advanced-fluid region pin. Current policy tests cover default water, dormant authored regions, save staging and export. Existing unsupported region/origin combinations still reject explicitly when enabled.

Use [Water V2 Workshop](../projects/water_v2_workshop/README.md) on Experimental for the accepted physical/presentation workflow. Basic Water remains independent. This architectural cleanup does not change the accepted solver or unlock Alpha.
