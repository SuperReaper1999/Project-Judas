"""Infinite aligned-lattice counterexample, independent of wall/solver tolerances."""
import math

spacing = 0.20
support = 0.42
mass = 8.0
rest = 1000.0
occupancy = 4
w0 = 315 / (64 * math.pi * support**3)
ratio = ((rest / (mass * occupancy * w0) - 1) / 6) ** (1 / 3)
separation = support * math.sqrt(1 - ratio)
assert separation < support < math.sqrt(2) * separation
sampled = mass * occupancy * w0 * (1 + 6 * (1 - separation**2 / support**2)**3)
true_bulk = mass * occupancy / separation**3
raw_volume_coverage = (spacing / separation)**3
print(f"cluster_center_spacing={separation:.12f} m")
print(f"sampled_density={sampled:.12f} kg/m^3")
print(f"bulk_mass_density={true_bulk:.12f} kg/m^3")
print(f"raw_volume_coverage={raw_volume_coverage:.12f}")
print("Each site has 4 coincident particles and 6 neighboring sites in kernel reach.")
print("At any site, all kernel gradients vanish or cancel by reflection symmetry.")
print("At zero velocity and gravity, the one-sided density QP has zero minimum change.")
