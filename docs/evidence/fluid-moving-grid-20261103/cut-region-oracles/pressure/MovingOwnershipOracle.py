"""Two pressure owners with exact sliver/cap volume and wall-work coefficients."""
from decimal import Decimal, getcontext
from pathlib import Path
import struct

getcontext().prec = 70
D = Decimal
root = Path(__file__).parent
source = (root.parent.parent / "remap-native-continuation/failed/native-root-input.bin").read_bytes()
lo_y, hi_y, dt = [D(struct.unpack_from("<d", source, k*8)[0]) for k in (1, 4, 10)]
h = D(".125")
descent = D(float(.0036349140061924913))
rho = D(1000)
p_out, p_other = D(1000), D(0)
retained_predictor = D(float(-.43292592362295285))
old_half_mass = D(".9765625")
raw_flux = dt * h * h * retained_predictor
inlet_c = (dt * h * h) ** 2 / old_half_mass
closed_normal_reaction = p_out + raw_flux / inlet_c
tol = D("1e-55")


def check(a, b, description):
    assert abs(a-b) < tol, (description, a, b)


def geometry(width, cap_opening):
    if cap_opening:
        c0, c1 = D(0), 6*h - hi_y + descent
        t0 = (hi_y - 6*h) / descent
        cap_time = 1-t0
        slit_time_integral = (1-t0*t0) / 2
        c_avg = c1 * cap_time / 2
        dc = descent
    else:
        c0, c1 = lo_y - 3*h, D(0)
        t0 = c0 / descent
        cap_time = t0
        slit_time_integral = t0*t0 / 2
        c_avg = c0 * cap_time / 2
        dc = -descent
    cap_mouth = h * c_avg
    top_mouth = h*h * cap_time
    throat = h*width*slit_time_integral
    wall_area = h*h - cap_mouth
    wall_slit = width * wall_area
    wall_cap = dc * (top_mouth - throat)
    reclass = dc * throat
    physical_slit_delta = width * (h-c1) * h
    physical_cap_delta = h*h*(c1-c0)
    check(physical_slit_delta, wall_slit-reclass, "moving partition slit balance")
    check(physical_cap_delta, wall_cap+reclass, "moving partition cap balance")
    return cap_mouth, top_mouth, throat, wall_area, wall_slit, wall_cap, reclass


def simulate(width, cap_opening, preserve_wall_episode):
    cap_mouth, top_mouth, throat, wall_area, j_slit, j_cap, j_reclass = geometry(width, cap_opening)
    g_cap = dt*dt*cap_mouth/(rho*h)
    g_end = dt*dt*top_mouth/(rho*h)
    g_neck = dt*dt*throat/(rho*h/2)
    start_flux = raw_flux if preserve_wall_episode else D(0)
    if width:
        matrix_a = inlet_c + g_neck
        matrix_b = -g_neck
        matrix_c = g_cap + g_neck + g_end
        rhs_slit = inlet_c*p_out + start_flux - j_slit
        rhs_cap = g_cap*p_out + g_end*p_other - j_cap
        det = matrix_a*matrix_c - matrix_b*matrix_b
        p_slit = (rhs_slit*matrix_c - matrix_b*rhs_cap)/det
        p_cap = (matrix_a*rhs_cap - matrix_b*rhs_slit)/det
        side = start_flux + inlet_c*(p_out-p_slit)
    else:
        # When the slit vanishes there is no slit pressure DOF. Use the
        # same normal reaction across the closed wall episode.
        p_slit = p_out + start_flux / inlet_c
        p_cap = (g_cap*p_out + g_end*p_other - j_cap)/(g_cap + g_end)
        side = D(0)
    neck = g_neck*(p_slit-p_cap)
    direct = g_cap*(p_out-p_cap)
    top = g_end*(p_cap-p_other)
    check(side-neck, j_slit, "slit has only physical solid sweep demand")
    check(direct+neck-top, j_cap, "cap has only physical solid sweep demand")
    actual_relative = neck + j_reclass
    check(side-actual_relative, j_slit-j_reclass, "relative volume crosses changing partition")
    check(direct+actual_relative-top, j_cap+j_reclass, "changing partition transfer is paired")
    wall_work = p_slit*j_slit+p_cap*j_cap
    edge_work = (p_out-p_slit)*side + (p_slit-p_cap)*neck + (p_out-p_cap)*direct + (p_cap-p_other)*top
    check(p_out*(side+direct)-p_other*top-edge_work, wall_work,
          "wall/pressure work uses physical sweep only")
    return neck, p_slit*wall_area, wall_work


print("orientation,width,cap direct mouth,throat,wall force,fluid neck,wall work")
for opening in (True, False):
    for retained in (False, True):
        zero = simulate(D(0), opening, retained)
        coarse = simulate(D("1e-8"), opening, retained)
        fine = simulate(D("1e-14"), opening, retained)
        assert 0 < abs(fine[0]/coarse[0]) < D(".00000101")
        assert 0 < abs((fine[1]-zero[1])/(coarse[1]-zero[1])) < D(".00000101")
        if retained:
            old_wall = p_out*geometry(D(0), opening)[3]
            assert abs(fine[1] - old_wall) > 90
        for width, vals in ((D(0), zero), (D("1e-8"), coarse), (D("1e-14"), fine)):
            cap, _, throat, *_ = geometry(width, opening)
            print("top birth" if opening else "bottom death", retained, width, f"{cap:.12g}",
                  f"{throat:.12g}", f"{vals[1]:.12g}", f"{vals[0]:.12g}", f"{vals[2]:.12g}", sep=",")
print(f"dead-end zero-flux pressure with retained predictor {closed_normal_reaction:.12g}")
print("PASS exact swept wall volume, moving partition conservation, pressure work and continuity in both cap episodes")
