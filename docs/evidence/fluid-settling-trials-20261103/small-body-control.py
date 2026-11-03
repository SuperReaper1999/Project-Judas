"""Calm continuum control for the unchanged 0.12 m box (no particle feedback)."""
import csv
import math
import pathlib

root = pathlib.Path(__file__).resolve().parents[1]
source = root / "input-devices-20261103/production-final/small_mass_125x.csv"
with source.open() as stream:
    rows = list(csv.DictReader(stream))
height = 1.849942543
half = 0.06000000238
g = 9.81
rate = 2.0

def wet(y):
    return min(max((height-y+half)/(2*half), 0), 1)

def rhs(y, v):
    f = wet(y)
    return v, g*(2*f-1) - rate*f*v

def rk4(dt, duration):
    y, v = 0.6999999881, 0.
    for i in range(round(duration/dt)):
        a, b = rhs(y, v)
        c, d = rhs(y+a*dt/2, v+b*dt/2)
        e, f = rhs(y+c*dt/2, v+d*dt/2)
        h, j = rhs(y+e*dt, v+f*dt)
        y += dt*(a+2*c+2*e+h)/6
        v += dt*(b+2*d+2*f+j)/6
    return y, v

for d in (1/60,1/600,1/6000,1/60000):
    print(f"calm_reference step={d:.12g} y4={rk4(d,4)[0]:.9f} vy4={rk4(d,4)[1]:.9f} y8={rk4(d,8)[0]:.9f} vy8={rk4(d,8)[1]:.9f}")
for step in [0,9,24,48,72,96,120,144,168,192,216,239]:
    row = rows[step]
    y, v = float(row['y']), float(row['vy'])
    fraction, fy = float(row['immersion']), float(row['bfy'])
    prev = float(rows[step-1]['vy']) if step else 0.
    inferred_drag = (v-prev)*60*.864 + .864*g-fy
    nonhydro_accel = fy/(.864*2*fraction)-g if fraction > .001 else math.nan
    print(f"record t={(step+1)/60:.3f} y={y:.6f} vy={v:.6f} wet={fraction:.6f} buoyancy={fy:.6f} drag_other={inferred_drag:.6f} field_load_excess_per_wet_mass={nonhydro_accel:.6f}")
