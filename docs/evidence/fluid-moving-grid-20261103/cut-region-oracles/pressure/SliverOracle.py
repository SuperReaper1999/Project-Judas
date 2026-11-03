"""Independent reduced pressure-path oracle on the captured cell's geometry.

The uniform water phase and fixed external pressure difference are intentional.
This is a geometric/constitutive control, not a substitute for the captured
mixed predictor or an integration of moving-boundary work.
"""
from decimal import Decimal, getcontext
from pathlib import Path
import struct

getcontext().prec = 70
D = Decimal
root = Path(__file__).parent
source = (root.parent.parent / "remap-native-continuation/failed/native-root-input.bin").read_bytes()
_, hi_y, _, dt = map(D, struct.unpack_from("<3d", source, 3 * 8) + struct.unpack_from("<d", source, 10 * 8))
h = D(".125")
dy = D(float(-.0036349140061924913))
rho = D(1000)
p_out, p_upper = D(1000), D(0)
cross_y = 6 * h
tau = (hi_y - cross_y) / -dy
assert 0 < tau < 1

# x2/y5/z4: the upper cap appears only after normalized time tau. Its
# height is zero beforehand and grows linearly to y_face - hi_y(1).
cap_end = cross_y - hi_y - dy
cap_x_area = h * cap_end * (1 - tau) / 2
cap_y_area = h * h * (1 - tau)
wall_area = h * h - cap_x_area

def conductance(area, length):
    return dt * dt * area / (rho * length)

def solve(eps, model):
    # The lateral slit has width eps*t. Only its cross section present
    # during the cap episode can participate in the slit-to-cap branch.
    neck_area = h * eps * (1 - tau * tau) / 2
    c_direct = conductance(cap_x_area, h)
    c_top = conductance(cap_y_area, h)
    c_outer = conductance(wall_area, h / 2 + eps / 2) if eps else D(0)
    c_throat = conductance(neck_area, (h - cap_end) / 2)
    if model in ("coarse", "traction-only") and eps:
        c_direct = conductance(h * h, h)
        c_series = D(0)
    else:
        c_series = (c_outer * c_throat / (c_outer + c_throat)) if eps else D(0)
    x_net = c_direct + c_series
    q = x_net * c_top / (x_net + c_top) * (p_out - p_upper)
    p_cap = p_upper + q / c_top
    q_direct = c_direct * (p_out - p_cap)
    q_slit = c_series * (p_out - p_cap)
    p_slit = p_out - (q_slit / c_outer if eps else 0)
    work = (p_out - p_upper) * q
    dissipation = ((p_out - p_cap) * q_direct + (p_out - p_slit) * q_slit +
                   (p_slit - p_cap) * q_slit + (p_cap - p_upper) * q)
    assert abs(q - q_direct - q_slit) < D("1e-60")
    assert abs(work - dissipation) < D("1e-55") and work > 0
    if not eps or model == "traction-only":
        p_traction = p_out
    elif model == "coupled":
        p_traction = p_slit
    else:
        p_traction = p_cap
    return q, p_traction * wall_area, q_slit, neck_area

def scientific(value):
    return f"{value:.13E}"

print("input cell h, dt, cap activation t/h, cap depth, direct x mouth, top y mouth")
print(*(scientific(x) for x in (h, dt, tau, cap_end, cap_x_area, cap_y_area)))
print("model,eps,mean_throat_area,total_Q,side_branch_Q,traction")
models = ("coarse", "throat-only", "traction-only", "coupled")
for eps in map(D, ("0", "1e-4", "1e-6", "1e-8", "1e-10", "1e-12", "1e-14")):
    for model in models:
        q, force, slit, neck = solve(eps, model)
        print(model, *(scientific(x) for x in (eps, neck, q, slit, force)), sep=",")

q0, f0, *_ = solve(D(0), "coupled")
q_ref, f_ref, slit_ref, _ = solve(D("1e-8"), "coupled")
eps = D("1e-14")
q, f, slit, _ = solve(eps, "coupled")
assert 0 < slit / slit_ref < D(".00000101")
assert 0 < (q-q0) / (q_ref-q0) < D(".00000101")
assert 0 < (f0-f) / (f0-f_ref) < D(".00000101")
q_bad, f_bad, *_ = solve(eps, "coarse")
assert q_bad / q0 > 30 and abs(f_bad-f0) > D(5)
q_throat, f_throat, *_ = solve(eps, "throat-only")
assert q_throat == q and abs(f_throat-f0) > D(5)
q_traction, f_traction, *_ = solve(eps, "traction-only")
assert q_traction == q_bad and f_traction == f0
print("PASS continuity, constitutive flow, three independent failure controls, and graph work")
