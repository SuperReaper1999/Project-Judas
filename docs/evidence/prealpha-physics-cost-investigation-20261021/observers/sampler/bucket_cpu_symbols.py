#!/usr/bin/env python3
"""Disjoint, source-owned direct-PC buckets; no stacks or inferred callers.

First matching rule wins. Generic math/memory/container functions without an
identifiable owner remain unclassified. A function's own instructions may
include inlined callees; these buckets do not claim to recover those callees.
"""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import re


def rule(identity, bucket, field, pattern, sources, explanation):
    return {"id": identity, "bucket": bucket, "field": field, "pattern": pattern,
            "sources": sources, "explanation": explanation}


RULES = [
    rule("allocator_named", "allocator", "symbol",
         r"^(?:operator (?:new|delete)\b|std::pmr::(?:unsynchronized_pool_resource|monotonic_buffer_resource)::|ContactSolver::FrameUpstream::do_(?:de)?allocate)",
         ["src/ContactSolver.cpp:64", "system C++ allocation symbols"], "Explicit allocation-resource implementation; not every container helper."),
    rule("allocator_libc_source", "allocator", "representative_location", r"(?:^|/)malloc(?:/|\.c)",
         ["system libc malloc/malloc.c"], "PC resolved inside libc allocator implementation."),
    rule("bounds_owned_tu", "bounds_candidates_sort", "source_file", r"^(?:WorldBounds|Broadphase)\.cpp$",
         ["src/physics/WorldBounds.cpp:5", "src/Broadphase.cpp:6"], "Bounds/broadphase TU, including specialized sort helpers reported as owned by it."),
    rule("bounds_named", "bounds_candidates_sort", "symbol",
         r"^(?:PhysicsWorld::Impl::(?:Orientation|CurrentBound|PreviousBound|TightBound|StepReach|CoverStepReach|RefreshProxy|QuerySlots|GenerateCandidatePairs)\(|DynamicAabbTree::|ShapeAabb\(|PrepareShapeBounds\()",
         ["src/physics/WorldBounds.cpp:5", "src/physics/WorldBounds.cpp:120", "src/physics/WorldBounds.cpp:127", "src/Narrowphase.cpp:215", "src/Narrowphase.cpp:242", "src/Broadphase.cpp:6"],
         "Named bounds/candidate methods, tree implementation and explicit shape-bound preparation."),
    rule("geometry_owned_tu", "native_contact_geometry_interval", "source_file",
         r"^(?:Contacts|Narrowphase|CollisionGeometry)\.cpp$|^(?:ContactGeometryInternal|ContactSatFilter|ContactPreparedGeometry)\.h$",
         ["src/Contacts.cpp:599", "src/Narrowphase.cpp:10", "src/CollisionGeometry.cpp", "src/ContactGeometryInternal.h", "src/ContactSatFilter.h"],
         "Native contact geometry TU/header after the explicit shape-bounds rule."),
    rule("geometry_named", "native_contact_geometry_interval", "symbol",
         r"^(?:contact_geometry::|rigid_motion_bounds::|(?:double|float|bool|contact_geometry::[A-Za-z_]+|std::array<contact_geometry::Iv, 3ul>) contact_geometry::|ContactPreparedOrientation::|ContactSatFilter::|SpherePredicateFilter::|ContactRotation\(|Primitive(?:At|Count|Contacts|HasContacts)\(|ComputeContacts\(|SphereVs|BoxVs|ClosestPoint|CapsuleDistance)",
         ["src/Contacts.cpp:577", "src/Contacts.cpp:599", "src/Narrowphase.cpp:10", "src/ContactGeometryInternal.h", "src/RigidMotionBounds.h:25"],
         "Direct geometry, predicates and interval-bound functions; not arbitrary functions whose template arguments merely mention geometry types."),
    rule("response_owned_tu", "constraint_response", "source_file", r"^(?:ContactSolver|ImpactSolver|JointSolver)\.cpp$",
         ["src/ContactSolver.cpp:253", "src/ImpactSolver.cpp:53", "src/ImpactSolver.cpp:155", "src/JointSolver.cpp"],
         "Constraint/impact response implementation; called discovery callbacks retain their own symbol ownership."),
    rule("response_named", "constraint_response", "symbol", r"^(?:ContactSolver|ImpactSolver|JointSolver)::",
         ["src/ContactSolver.cpp:88", "src/ContactSolver.cpp:305", "src/ImpactSolver.cpp:155", "src/JointSolver.cpp"],
         "Explicit response methods, including response-local frame registration and row preparation."),
    rule("event_owned_tu", "event_contact_domain_processing", "source_file",
         r"^(?:WorldImpactEvents|WorldIncrementalImpact|WorldCCDSearch|WorldContacts)\.cpp$",
         ["src/physics/WorldImpactEvents.cpp:112", "src/physics/WorldIncrementalImpact.cpp:5", "src/physics/WorldCCDSearch.cpp:546", "src/physics/WorldContacts.cpp:77"],
         "Event/domain/contact orchestration TU, including owner-specific sorting/container helpers. Pure geometry namespace rules win earlier."),
    rule("event_named", "event_contact_domain_processing", "symbol",
         r"^(?:PhysicsWorld::Impl::(?:ResolveInitialImpacts|AdvanceImpacts|TryIncrementalImpact|PrepareInitialContactGeometry|Observe|FinishTouches|RemainingImpactBound|NormalSpeed|DriftGenerator|PlaneDrift|StaticMeshPolyPair|MeshDepthGrowthSafe|PrimitiveFeature|SpinsThroughSupport|SupportContinuationSafe|SpinPenetrationTarget|ImpactTime|GeometryPoseAt|ActualImpactBound|PairAt)\(|JudasPhysicsDetail::ContactKey::)|JudasPhysicsDetail::Impact(?:EventSchedule|CandidateCache)",
         ["src/physics/WorldCCDSearch.cpp:5", "src/physics/WorldCCDSearch.cpp:546", "src/physics/WorldImpactQueries.h:16", "src/physics/WorldImpactQueries.h:87", "src/physics/ImpactEventSchedule.h:18", "src/physics/ImpactCandidateCache.h:14"],
         "Named query/event orchestration and containers specialized on the explicit event schedule/cache types. No inference for generic key-only std algorithms."),
    rule("physics_coordinator_named", "physics_coordinator_other", "symbol",
         r"^(?:PhysicsWorld::|RigidMotion::|RigidMotionSegment::|AngularDriftGenerator\(|TransportAngularVelocity\(|IntegrateRigidBody|DynamicBody::)",
         ["src/PhysicsWorld.cpp", "src/RigidMotion.h:63", "src/RigidBody.cpp", "src/physics/WorldSleep.cpp"],
         "Other explicit physics ownership. Step self samples can contain inlined work and are not assigned to an assumed inner stage."),
    rule("physics_coordinator_tu", "physics_coordinator_other", "source_file", r"^PhysicsWorld\.cpp$|^(?:RigidBody|RigidMotion)\.(?:cpp|h)$",
         ["src/PhysicsWorld.cpp", "src/RigidBody.cpp", "src/RigidMotion.h"], "Other coordinator/motion TU-owned specialized helpers."),
    rule("game_runtime_named", "game_runtime_render", "symbol",
         r"^(?:RuntimeWorld::|Renderer::|ScriptSystem::|InputSystem::|ResourceManager::|EngineHost::|Window::|Application::|RuntimeUI::|AudioSystem::|GameSession::|InteractivePlay::|RuntimeDiagnostics::|Frustum::|JobSystem::|LiquidSystem::OpticalPaths\(|SphericalVolume::|DrawWorldGeometry\(|RenderWorldFrame\(|BuildRenderMaterialBindings\(|ResolveRenderMaterial\(|EncodeMaterialOverrides|PhysicalDigest::|digest$|JS_|js_|find_own_property|free_gc_object|add_shape_property|build_arg_list)",
         ["src/RuntimeWorld.cpp", "src/Renderer.cpp", "src/WorldPresentation.cpp", "src/ScriptSystem.cpp", "third_party/quickjs/quickjs.c", "private driver SampledLive.cpp"],
         "Named application/game-facing runtime, presentation, scripting and benchmark-observer work; not purely gameplay code."),
    rule("game_runtime_tu", "game_runtime_render", "source_file",
         r"^(?:WorldPresentation|WorldScripts|WorldCharacter|WorldAudio|WorldWeather|RuntimeWorld|ScriptSystem|RuntimeUI|Renderer|Material|ResourceManager|Simulation|SampledLive)\.cpp$|^quickjs\.c$",
         ["src/WorldPresentation.cpp", "src/WorldScripts.cpp", "src/Simulation.cpp", "src/RuntimeUI.cpp", "src/ResourceManager.cpp"],
         "Application/runtime/render/observer TU-owned specialized helpers."),
    rule("presentation_shared_image", "game_runtime_render", "path",
         r"/(?:lib(?:GL|EGL|glapi|gallium|nvidia|SDL|X11|Xext|xcb|wayland)[^/]*|[^/]*_dri\.so)",
         ["loaded image metadata"], "Direct CPU PCs in graphics/window/input platform libraries, not GPU execution time."),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("symbolized_directory", type=Path)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    output = args.output or args.symbolized_directory / "source-buckets"
    with (args.symbolized_directory / "symbols.csv").open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    compiled = [(item, re.compile(item["pattern"])) for item in RULES]
    counts = collections.defaultdict(lambda: {"window_samples": 0, "other_samples": 0, "symbols": 0})
    classified = []
    for row in rows:
        source = row["representative_location"].split(":", 1)[0].rsplit("/", 1)[-1]
        fields = dict(row, source_file=source)
        match = next((item for item, pattern in compiled if pattern.search(fields[item["field"]])), None)
        bucket = match["bucket"] if match else "unclassified"
        record = dict(row, bucket=bucket, rule=match["id"] if match else "no_provable_owner")
        classified.append(record)
        for key in ("window_samples", "other_samples"):
            counts[bucket][key] += int(row[key])
        counts[bucket]["symbols"] += 1
    total = sum(int(row["window_samples"]) for row in rows)
    for count in counts.values():
        count["window_percent"] = 100 * count["window_samples"] / total if total else 0
    assert sum(count["window_samples"] for count in counts.values()) == total
    ordered = sorted(counts, key=lambda name: -counts[name]["window_samples"])
    sources = {}
    for item in RULES:
        for source in item["sources"]:
            relative = source.split(":", 1)[0]
            path = args.repo / relative
            if path.is_file():
                sources[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
    summary = {"schema": "judas.disjoint-source-owned-pc-buckets.v1", "window_samples": total,
               "buckets": {name: counts[name] for name in ordered},
               "rules_first_match_wins": RULES, "source_fingerprints": sources,
               "unclassified_top": [row for row in classified if row["bucket"] == "unclassified"][:15],
               "limitations": ["Disjoint direct instruction-pointer samples, never summed inclusive profiler spans.",
                               "No call stacks: generic libc/libm/container functions without proven owner remain unclassified.",
                               "Reported TU ownership identifies the emitted function, not the callers of shared specializations.",
                               "Optimized functions can include inlined work; these rules do not recover folded inlined callees.",
                               "WorldCCDSearch planning is event/domain processing; its separately named interval/geometry functions belong to native geometry.",
                               "Physics coordinator self samples are deliberately separate rather than assigned to guessed inner stages.",
                               "Sample tags retain last-completed-step semantics; rendering and observation can share a physics step tag."]}
    output.mkdir(parents=True, exist_ok=True)
    with (output / "classified_symbols.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(classified[0]) if classified else [])
        writer.writeheader()
        writer.writerows(classified)
    (output / "buckets.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({"output": str(output), "window_samples": total, "buckets": summary["buckets"]}, indent=2))


if __name__ == "__main__":
    main()
