#!/usr/bin/env python3
"""Replay a production-fluid observation without calling its hydrostatic sampler.

This diagnoses the existing *exterior horizontal-plane* check; it does not infer
an unobserved, nonplanar free surface inside a displaced solid. An analytic box /
plane intersection independently checks the old 32^3 clipping quadrature. Neither
makes a remote plane a valid surface model. No result here unlocks fluids.

Capture with judas_production_fluid_tests --case half_025 --capture-immersion
--output DIR, then run this file DIR --case half_025 --spacing 0.25.
Only uniform-gravity fixtures are supported by this diagnostic.
Exit 0 means replay completed, NOT that production immersion passed. Consult
original_tolerance_pass in each reported window; missing frames are errors.
"""
from __future__ import annotations

import argparse
import csv
import itertools
import json
import math
from pathlib import Path


def dot(a, b):
    return math.fsum(x * y for x, y in zip(a, b))


def rotation_axes(quaternion):
    """Columns of a unit quaternion's rotation; preserve binary32 source values."""
    if len(quaternion) != 4 or not all(math.isfinite(v) for v in quaternion):
        raise ValueError("A finite unit quaternion is required")
    if abs(dot(quaternion, quaternion) - 1) > 2e-6:
        raise ValueError("A unit quaternion is required")
    w, x, y, z = quaternion
    return ((1 - 2 * (y*y + z*z), 2 * (x*y + w*z), 2 * (x*z - w*y)),
            (2 * (x*y - w*z), 1 - 2 * (x*x + z*z), 2 * (y*z + w*x)),
            (2 * (x*z + w*y), 2 * (y*z - w*x), 1 - 2 * (x*x + y*y)))


def box_below_plane_fraction(center, axes, half, normal, height):
    """Exact continuum fraction, up to binary64 rounding; no voxel quadrature.

A uniform box projects onto the sum of <=3 independent uniform intervals of
width 2*h_i*abs(n dot axis_i). Integrate their CDF by inclusion/exclusion. Reflect
at the midpoint to avoid subtracting large nearly equal terms near full wetness.
This computes geometry only: it contains no body density or desired fraction.
"""
    values = [*center, *half, *normal, height, *(v for axis in axes for v in axis)]
    if (len(center) != 3 or len(axes) != 3 or any(len(axis) != 3 for axis in axes)
            or len(half) != 3 or len(normal) != 3
            or not all(math.isfinite(v) for v in values)
            or min(half) <= 0 or dot(normal, normal) <= 0):
        raise ValueError("Finite box dimensions and a nonzero plane normal are required")
    for i, axis in enumerate(axes):
        for j, other in enumerate(axes):
            if abs(dot(axis, other) - (1 if i == j else 0)) > 4e-6:
                raise ValueError("Box axes must be orthonormal")
    # The analytic CDF's divided differences become ill-conditioned as an axis
    # tends to perpendicular. Do not silently drop a narrow dimension: reject
    # unresolved analytic evaluations rather than manufacture an exact result.
    widths = [2 * abs(dot(normal, axis)) * h for axis, h in zip(axes, half)]
    widths = [width for width in widths if width > 0]
    extent = math.fsum(widths)
    t = height - dot(center, normal) + extent * 0.5
    if t <= 0:
        return 0.0
    if t >= extent:
        return 1.0
    if abs(t - extent * 0.5) <= 8 * math.ulp(extent):
        return 0.5
    reflected = t > extent * 0.5
    if reflected:
        t = extent - t
    if min(widths) < 1e-7 * max(widths):
        raise ValueError("Near-perpendicular plane requires a higher-precision clipping oracle")
    degree = len(widths)
    terms = [(-1) ** sum(subset) * max(0.0, t - dot(widths, subset)) ** degree
             for subset in itertools.product((0, 1), repeat=degree)]
    result = math.fsum(terms) / (math.factorial(degree) * math.prod(widths))
    if not math.isfinite(result) or result < -1e-9 or result > 1 + 1e-9:
        raise ValueError("Ill-conditioned analytic box/plane intersection")
    return 1 - result if reflected else result


def exterior_plane(particles, center, half, spacing, span=2.0):
    """Legacy fixture estimator, in its authored uniform-gravity reservoir frame.

Deliberately retain its cross-shaped exclusion and upper median. This is a
reproduction of the failed observation, NOT a proposed correct surface oracle.
"""
    if not math.isfinite(spacing) or spacing <= 0 or not math.isfinite(span) or span <= 0:
        raise ValueError("Positive finite spacing and reservoir span are required")
    if not all(math.isfinite(v) for v in (*center, *half)) or min(half) <= 0:
        raise ValueError("Invalid observer box")
    count = round(span / spacing)
    if count < 1 or count > 4096:
        raise ValueError("Unsupported observer grid")
    tops = {}
    for x, y, z in particles:
        if not all(math.isfinite(v) for v in (x, y, z)):
            raise ValueError("Nonfinite particle position")
        if abs(x-center[0]) < half[0]+spacing or abs(z-center[2]) < half[2]+spacing:
            continue
        ix, iz = math.floor((x+span*.5)/spacing), math.floor((z+span*.5)/spacing)
        if 0 <= ix < count and 0 <= iz < count:
            tops[ix, iz] = max(tops.get((ix, iz), -math.inf), y + spacing*.5)
    if not tops:
        raise ValueError("No exterior observations")
    values = sorted(tops.values())
    return values[len(values)//2], len(values), values[0], values[-1]


def read_snapshot(directory, case, stage):
    with (directory / f"{case}_{stage}_body.csv").open(encoding="utf-8", newline="") as stream:
        body = next(csv.DictReader(stream))
    with (directory / f"{case}_{stage}_particles.csv").open(encoding="utf-8", newline="") as stream:
        particles = [tuple(float(row[k]) for k in ("x", "y", "z"))
                     for row in csv.DictReader(stream)]
    return body, particles


def analyze(directory, case, spacing):
    with (directory / f"{case}_observations.csv").open(encoding="utf-8", newline="") as stream:
        observations = list(csv.DictReader(stream))
    if not observations:
        raise ValueError("No observation frames were captured")
    expected = {"original": set(range(181, 241))}
    contract_path = directory / f"{case}_settling.json"
    if contract_path.exists():
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
        expected["late"] = set(range(contract["late_first_step"]+1, contract["late_end_step"]+1))
    actual = {}
    for row in observations:
        step, window = int(row["step"]), row["window"]
        if window not in expected or step in actual.setdefault(window, set()):
            raise ValueError("Unexpected window or duplicate observation frame")
        actual[window].add(step)
        for key in ("begin_y", "end_y", "applied_fraction", "independent_fraction", "surface_y"):
            if not math.isfinite(float(row[key])):
                raise ValueError("Nonfinite or unavailable observation")
        if not 0 <= float(row["independent_fraction"]) <= 1 or row["fluid_updated"] not in ("0", "1"):
            raise ValueError("Invalid fraction or fluid-update flag")
    if actual != expected:
        raise ValueError("Incomplete observation windows; refusing a partial-window mean")
    results = []
    for row in observations:
        stage = "observe" + row["step"]
        body, particles = read_snapshot(directory, case, stage)
        center = tuple(float(body[k]) for k in ("x", "y", "z"))
        half = tuple(float(body[k]) for k in ("hx", "hy", "hz"))
        gravity = tuple(float(body[k]) for k in ("gx", "gy", "gz"))
        if abs(gravity[0]) > 1e-6 or abs(gravity[2]) > 1e-6 or gravity[1] >= 0:
            raise ValueError("Replay currently supports only authored -Y uniform-gravity fixtures")
        # Assert this is the supported uniform fixture, rather than interpreting
        # a radial field with the same local up as a single plane.
        if results and abs(gravity[1] - results[0]["gravity_y"]) > 1e-6:
            raise ValueError("Changing gravity is outside this plane replay's scope")
        surface, columns, low, high = exterior_plane(particles, center, half, spacing)
        if abs(surface-float(row["surface_y"])) > 1e-6:
            raise ValueError("Native and replayed surface estimates disagree")
        quaternion = tuple(float(body[k]) for k in ("qw", "qx", "qy", "qz"))
        analytic = box_below_plane_fraction(center, rotation_axes(quaternion), half, (0, 1, 0), surface)
        before, before_particles = read_snapshot(directory, case, "before" + row["step"])
        before_center = tuple(float(before[k]) for k in ("x", "y", "z"))
        if abs(before_center[1]-float(row["begin_y"])) > 1e-6 or abs(center[1]-float(row["end_y"])) > 1e-6:
            raise ValueError("Observation and snapshot timestamps disagree")
        before_quaternion = tuple(float(before[k]) for k in ("qw", "qx", "qy", "qz"))
        before_surface = exterior_plane(before_particles, before_center, half, spacing)[0]
        before_fraction = box_below_plane_fraction(
            before_center, rotation_axes(before_quaternion), half, (0, 1, 0), before_surface)
        results.append({"step": int(row["step"]), "window": row["window"],
                        "native_fraction": float(row["independent_fraction"]),
                        "analytic_plane_fraction": analytic, "surface_y": surface,
                        "analytic_plane_fraction_at_step_start": before_fraction,
                        "exterior_columns": columns, "exterior_height_span": high-low,
                        "applied_fraction_at_step_start": float(row["applied_fraction"]),
                        "rigid_center_height_change": float(row["end_y"])-float(row["begin_y"]),
                        "fluid_updated": row["fluid_updated"] == "1", "gravity_y": gravity[1]})
    windows = {}
    for name in sorted({row["window"] for row in results}):
        rows = [row for row in results if row["window"] == name]
        average = lambda key: math.fsum(row[key] for row in rows)/len(rows)
        windows[name] = {"samples": len(rows), "native_mean": average("native_fraction"),
                         "analytic_plane_mean": average("analytic_plane_fraction"),
                         "analytic_plane_mean_at_step_start": average("analytic_plane_fraction_at_step_start"),
                         "maximum_start_end_plane_difference": max(abs(row["analytic_plane_fraction"]-row["analytic_plane_fraction_at_step_start"]) for row in rows),
                         "maximum_plane_quadrature_error": max(abs(row["native_fraction"]-row["analytic_plane_fraction"]) for row in rows),
                         "maximum_exterior_height_span": max(row["exterior_height_span"] for row in rows),
                         "maximum_center_step_distance": max(abs(row["rigid_center_height_change"]) for row in rows),
                         "mean_applied_fraction_at_step_start": average("applied_fraction_at_step_start"),
                         "original_tolerance_pass": abs(average("native_fraction")-.5) <= .1}
    return {"scope": "Diagnostic of the original remote-plane observation; no nonplanar oracle or unlock claim",
            "windows": windows, "frames": results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--case", required=True)
    parser.add_argument("--spacing", type=float, required=True)
    args = parser.parse_args()
    if Path(args.case).name != args.case or args.case in (".", ".."):
        parser.error("Case must be a filename stem")
    try:
        print(json.dumps(analyze(args.directory, args.case, args.spacing), indent=2, allow_nan=False))
    except (ValueError, OSError, KeyError, StopIteration) as error:
        parser.exit(2, f"Immersion replay failed: {error}\n")


if __name__ == "__main__":
    main()
