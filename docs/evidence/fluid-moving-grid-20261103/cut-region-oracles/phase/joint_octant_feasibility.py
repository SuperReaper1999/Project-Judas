#!/usr/bin/env python3
"""Disposable, sparse fixed-primal LP for one exported .20 trial."""
import argparse
import hashlib
from collections import deque
from pathlib import Path
import time

import numpy as np
from scipy.optimize import linprog
from scipy.sparse import coo_array, eye_array, hstack


def read_sample(path):
    with path.open() as f:
        label, n, tolerance, witness = f.readline().split()
        assert label == "sample"
        n, witness, tolerance = int(n), int(witness), float(tolerance)
        water = np.zeros(n)
        capacity = np.zeros((n, 8))
        marginal = np.zeros((n, 3))
        common = np.full((n, 8), np.nan)
        half = np.zeros((3, 2, n, 2, 4))  # start, net axial, axial out, relay
        trans, mid = [], []
        for line in f:
            tag, *data = line.split()
            if tag == "cell":
                i = int(data[0])
                water[i] = float(data[1])
                capacity[i] = [float(v) for v in data[2:10]]
                marginal[i] = [float(v) for v in data[10:13]]
                if len(data) > 13 and int(data[13]):
                    common[i] = [float(v) for v in data[14:22]]
            elif tag == "half":
                a, p, i = map(int, data[:3])
                half[a, p, i] = np.array([float(v) for v in data[3:]]).reshape(4, 2).T
            elif tag == "trans":
                trans.append((*map(int, data[:4]), *map(float, data[4:])))
            elif tag == "mid":
                mid.append((*map(int, data[:3]), *map(float, data[3:])))
            else:
                raise ValueError(tag)
    return n, witness, tolerance, water, capacity, marginal, common, half, trans, mid


class Matrix:
    def __init__(self, rhs):
        self.rhs = rhs
        self.r, self.c, self.v = [], [], []

    def entry(self, row, col, value):
        self.r.append(row)
        self.c.append(col)
        self.v.append(value)

    def sparse(self, cols):
        return coo_array((self.v, (self.r, self.c)),
                         shape=(len(self.rhs), cols)).tocsc()


def solve(sample, mode, scale, radius=None, witness_path=None, explicit_patch=None, eliminate_fixed=False):
    n, witness, tol, water, capacity, marginal, common, half, trans, mid = sample
    impose_marginal = mode == "independent-picks"
    donors = mode != "joint-without-donor-budgets"
    whole = 12 * n
    marginal_base = whole + n
    row_count = marginal_base + (3 * n if impose_marginal else 0)
    eq = Matrix(np.zeros(row_count))
    donor = Matrix(np.zeros(whole))
    active_donor = np.zeros(whole, dtype=bool)
    bounds, reference, owners = [], [], []
    trace_edges = [[[] for _ in range(2)] for _ in range(3)]

    def variable(lo, hi, old=np.nan, cells=()):
        result = len(bounds)
        bounds.append((lo, hi))
        reference.append(old)
        owners.append(cells)
        return result

    def row(a, p, cell, side):
        return ((a * 2 + p) * n + cell) * 2 + side

    def budget(a, p, cell, side, var, coef, fixed=0.):
        index = row(a, p, cell, side)
        if active_donor[index]:
            donor.entry(index, var, coef)
            donor.rhs[index] -= fixed

    for a in range(3):
        for p in range(2):
            for cell in range(n):
                for side in range(2):
                    i = row(a, p, cell, side)
                    start, axial, outgoing, relay = half[a, p, cell, side]
                    eq.rhs[i] = start + axial
                    donor.rhs[i] = start - outgoing
                    active_donor[i] = donors and relay == 0.

    for cell in range(n):
        eq.rhs[whole + cell] = water[cell]
        for j in range(8):
            cap = capacity[cell, j]
            var = variable(0., cap, common[cell, j], (cell,))
            eq.entry(whole + cell, var, 1.)
            for a in range(3):
                side = (j >> a) & 1
                eq.entry(row(a, 0, cell, side), var, 1.)
                eq.entry(row(a, 1, cell, side), var, -1.)
                eq.rhs[row(a, 1, cell, side)] -= cap
                if impose_marginal and side == 0:
                    i = marginal_base + a * n + cell
                    eq.entry(i, var, 1.)
                    eq.rhs[i] = marginal[cell, a]

    for a, p, left, right, q, lo, hi, _preferred, *reference_low in trans:
        var = variable(lo, hi, reference_low[0] if reference_low else np.nan, (left, right))
        eq.entry(row(a, p, left, 0), var, 1.)
        eq.entry(row(a, p, right, 0), var, -1.)
        eq.entry(row(a, p, left, 1), var, -1.)
        eq.entry(row(a, p, right, 1), var, 1.)
        eq.rhs[row(a, p, left, 1)] -= q
        eq.rhs[row(a, p, right, 1)] += q
        source, orientation = (left, 1.) if q > 0. else (right, -1.)
        budget(a, p, source, 0, var, orientation)
        budget(a, p, source, 1, var, -orientation, abs(q))
        trace_edges[a][p].append(("trans", left, right, var, q))

    for a, p, cell, lo, hi, *reference_flow in mid:
        assert lo <= 0. <= hi
        old = reference_flow[0] if reference_flow else np.nan
        pos, neg = variable(0., hi, max(old, 0.), (cell,)), variable(0., -lo, max(-old, 0.), (cell,))
        eq.entry(row(a, p, cell, 0), pos, 1.)
        eq.entry(row(a, p, cell, 1), pos, -1.)
        eq.entry(row(a, p, cell, 0), neg, -1.)
        eq.entry(row(a, p, cell, 1), neg, 1.)
        budget(a, p, cell, 0, pos, 1.)
        budget(a, p, cell, 1, neg, 1.)
        trace_edges[a][p].append(("mid", cell, cell, pos, neg))

    physical_variables = len(bounds)
    physical_eq = eq.sparse(physical_variables)
    physical_donor = donor.sparse(physical_variables)
    rounding = 32 * np.finfo(float).eps * np.max(np.sum(capacity, axis=1))
    headroom = max(0., tol - rounding)
    if radius is not None or explicit_patch is not None:
        assert n == 1500 and mode == "joint"
        old = np.asarray(reference)
        box = np.asarray(bounds)
        defined = np.isfinite(old)
        if not np.all(defined[8*n:]):
            raise ValueError("local sample lacks independent half transfers")
        old_finite = np.nan_to_num(old)
        row_cells = np.concatenate(((np.arange(whole) // 2) % n, np.arange(n)))
        eq_defect = np.abs(physical_eq @ old_finite - eq.rhs)
        donor_defect = physical_donor @ old_finite - donor.rhs
        mandatory = set(np.flatnonzero(~np.all(np.isfinite(common), axis=1)).tolist())
        mandatory.update(row_cells[eq_defect > tol].tolist())
        donor_failing = np.flatnonzero(active_donor & (donor_defect > rounding))
        mandatory.update(row_cells[donor_failing].tolist())
        for k in np.flatnonzero(defined & ((old < box[:, 0]) | (old > box[:, 1]))):
            if max(box[k, 0] - old[k], old[k] - box[k, 1]) > rounding:
                mandatory.update(owners[k])

        def coordinate(cell):
            return cell % 10, (cell // 10) % 15, cell // 150

        def distance(cell):
            return sum(abs(a-b) for a, b in zip(coordinate(cell), coordinate(witness)))

        patch = ({cell for cell in range(n) if distance(cell) <= radius}
                 if explicit_patch is None else set(explicit_patch))
        outside_bad = sorted(mandatory - patch)
        print(f"radius={radius} explicitPatch={sorted(patch) if explicit_patch is not None else []} "
              f"patchCells={len(patch)} mandatoryCells={len(mandatory)} "
              f"donorBudgetFailures={len(donor_failing)} outsideInvalid={len(outside_bad)} "
              f"furthestMandatory={max(map(distance, mandatory), default=0)}")
        if outside_bad:
            print("cannot freeze invalid outside independent state",
                  [(cell, "no common octant witness" if not np.all(np.isfinite(common[cell]))
                    else float(np.max(eq_defect[row_cells == cell]))) for cell in outside_bad[:32]])
            return 6
        for j, cells in enumerate(owners):
            if all(cell in patch for cell in cells):
                continue
            if not defined[j]:
                raise ValueError("cannot freeze unavailable coherent reference")
            fixed = min(max(old[j], box[j, 0]), box[j, 1])
            bounds[j] = (fixed, fixed)

    mutable = np.array([lo < hi for lo, hi in bounds])
    movable_cells = {cell for can_move, cells in zip(mutable, owners) if can_move for cell in cells}
    mutable_eq_nz = physical_eq[:, mutable].nnz
    octant_end = 8 * n
    trans_end = octant_end + len(trans)
    print(f"mutablePhysicalVariables={int(np.sum(mutable))} "
          f"octants={int(np.sum(mutable[:octant_end]))} "
          f"transverse={int(np.sum(mutable[octant_end:trans_end]))} "
          f"directionalMidpoints={int(np.sum(mutable[trans_end:]))} "
          f"cellsTouchingMutable={len(movable_cells)} mutableEqNonzeros={mutable_eq_nz}")
    # Preserve the declared original balance tolerance with arithmetic
    # headroom. Slacks are numerical residuals, never material edges.
    donor_full = physical_donor.tocsr()[active_donor, :].tocsc()
    donor_rhs = donor.rhs[active_donor]
    physical_box = np.asarray(bounds)
    if eliminate_fixed:
        frozen = np.where(mutable, 0., physical_box[:, 0])
        eq_change = physical_eq[:, mutable].tocsr()
        eq_active = np.diff(eq_change.indptr) > 0
        remaining_eq = eq.rhs - physical_eq @ frozen
        donor_change = donor_full[:, mutable].tocsr()
        donor_active = np.diff(donor_change.indptr) > 0
        remaining_donor = donor_rhs - donor_full @ frozen
        if (np.any(np.abs(remaining_eq[~eq_active]) > tol) or
                np.any(remaining_donor[~donor_active] < -rounding)):
            print("fixed exterior still violates a declared balance or donor bound")
            return 6
        eq_count = int(np.sum(eq_active))
        a_eq = hstack((eq_change[eq_active], eye_array(eq_count))).tocsc()
        eq_rhs = remaining_eq[eq_active]
        a_ub = hstack((donor_change[donor_active],
                       coo_array((int(np.sum(donor_active)), eq_count)))).tocsc()
        b_ub = remaining_donor[donor_active]
        box = np.vstack((physical_box[mutable], np.tile([-headroom, headroom], (eq_count, 1))))
    else:
        for i in range(row_count):
            eq.entry(i, variable(-headroom, headroom), 1.)
        a_eq = eq.sparse(len(bounds))
        eq_rhs = eq.rhs
        a_ub = donor.sparse(len(bounds)).tocsr()[active_donor, :].tocsc()
        b_ub = donor_rhs
        box = np.asarray(bounds)
    size = len(box)
    print(f"mode {mode} cells={n} witness={witness} tolerance={tol:.17g} "
          f"columns={size} equality={a_eq.shape[0]} donor={a_ub.shape[0]} "
          f"nonzeros={a_eq.nnz + a_ub.nnz}", flush=True)
    began = time.perf_counter()
    result = linprog(np.zeros(size), A_ub=a_ub if a_ub.shape[0] else None,
                     b_ub=b_ub * scale if a_ub.shape[0] else None,
                     A_eq=a_eq, b_eq=eq_rhs * scale, bounds=box * scale,
                     method="highs",
                     options={"primal_feasibility_tolerance": 1e-9,
                              "dual_feasibility_tolerance": 1e-9,
                              "time_limit": 20.})
    print(f"status={result.status} elapsed_ms={(time.perf_counter()-began)*1000:.3f} "
          f"iterations={result.nit} {result.message}")
    if not result.success:
        return result.status
    candidate = result.x / scale
    solver_equality = np.max(np.abs(a_eq @ candidate - eq_rhs))
    if eliminate_fixed:
        x = frozen.copy()
        x[mutable] = candidate[:int(np.sum(mutable))]
    else:
        x = candidate[:physical_variables]
    before_clamp = max(np.max(physical_box[:, 0] - x), np.max(x - physical_box[:, 1]))
    x = np.clip(x, physical_box[:, 0], physical_box[:, 1])
    equality = np.max(np.abs(physical_eq @ x - eq.rhs))
    excess = np.max(donor_full @ x - donor_rhs) if donor_full.shape[0] else 0.
    bound = max(np.max(physical_box[:, 0] - x), np.max(x - physical_box[:, 1]))
    allocation = x[:8*n].reshape(n, 8)
    coherent = np.array([[sum(v[j] for j in range(8) if (j >> a) & 1 == 0)
                          for a in range(3)] for v in allocation])
    print(f"verified equation={equality:.17g} solverEquation={solver_equality:.17g} donorExcess={excess:.17g} "
          f"boundExcess={bound:.17g} rawBoundExcess={before_clamp:.17g} "
          f"largestMarginalChange={np.max(np.abs(coherent-marginal)):.17g}")
    print(f"witness octants {allocation[witness].tolist()}")
    print(f"witness independent lows {marginal[witness].tolist()} joint lows {coherent[witness].tolist()}")
    payload = x[:physical_variables].astype("<f8").tobytes()
    print("physical witness SHA-256", hashlib.sha256(payload).hexdigest())
    if witness_path is not None:
        np.savez_compressed(witness_path, physical=x[:physical_variables], octants=allocation,
                            marginals=coherent, tolerance=tol, raw_residual=equality)


    # Check independently whether positive phase transfers reach a real
    # material source. Tiny signed LP residuals are measured as such.
    if n == 1500:
        maximum_unowned = 0.
        for a, stride in enumerate((1, 10, 150)):
            for p in range(2):
                adjacency = [[] for _ in range(2*n)]
                for cell in range(n):
                    for side in range(2):
                        outgoing = half[a, p, cell, side, 2]
                        if outgoing > 0.:
                            recipient = 2*(cell + stride if side else cell - stride) + (1-side)
                            assert 0 <= recipient < 2*n
                            adjacency[2*cell+side].append((recipient, outgoing))
                for kind, left, right, var, value in trace_edges[a][p]:
                    def transfer(i, j, q):
                        if q > 0.:
                            adjacency[i].append((j, q))
                        elif q < 0.:
                            adjacency[j].append((i, -q))
                    if kind == "trans":
                        transfer(2*left, 2*right, x[var])
                        transfer(2*left+1, 2*right+1, value-x[var])
                    else:
                        transfer(2*left, 2*left+1, x[var]-x[int(value)])
                reached = {2*cell+side for cell in range(n) for side in range(2)
                           if half[a, p, cell, side, 0] > 0.}
                queue = deque(reached)
                while queue:
                    for target, q in adjacency[queue.popleft()]:
                        if q > 0. and target not in reached:
                            reached.add(target)
                            queue.append(target)
                maximum_unowned = max(maximum_unowned, max((q for i, neighbours in enumerate(adjacency)
                                      if i not in reached for _, q in neighbours), default=0.))
        print(f"maximum source-unreachable transfer {maximum_unowned:.17g}")
    return 0 if max(equality, excess, bound) <= tol else 5


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("sample", type=Path)
    parser.add_argument("mode", choices=["independent-picks", "joint", "joint-without-donor-budgets"])
    parser.add_argument("--scale", type=float, default=1e8)
    parser.add_argument("--radius", type=int)
    parser.add_argument("--patch", type=int, nargs="+")
    parser.add_argument("--eliminate-fixed", action="store_true")
    parser.add_argument("--witness", type=Path)
    options = parser.parse_args()
    raise SystemExit(solve(read_sample(options.sample), options.mode, options.scale, options.radius,
                           options.witness, options.patch, options.eliminate_fixed))
