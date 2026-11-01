"""Independent geometric controls for the fluid observation diagnostic.

These qualify plane clipping and expose a broken surface assumption. They do
not replace the still-failing production immersion gate.
"""
import importlib.util
import itertools
import json
import math
from fractions import Fraction
from pathlib import Path
import random
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location(
    'fluid_immersion_audit', Path(__file__).resolve().parents[1] /
    'tools/validation/audit_fluid_immersion.py')
OBSERVER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(OBSERVER)
IDENTITY = ((1., 0., 0.), (0., 1., 0.), (0., 0., 1.))


class ImmersionObserverTests(unittest.TestCase):
    def fraction(self, center=(0., 0., 0.), normal=(0., 1., 0.), height=0., axes=IDENTITY):
        return OBSERVER.box_below_plane_fraction(center, axes, (.3, .3, .3), normal, height)

    def test_horizontal_exact_controls_and_wrong_height(self):
        for y, expected in [(-1., 1.), (-.3, 1.), (-.15, .75), (0., .5), (.15, .25), (.3, 0.), (1., 0.)]:
            with self.subTest(y=y):
                self.assertAlmostEqual(self.fraction(center=(0., y, 0.)), expected, places=12)

    def test_diagonal_cube_has_known_irwin_hall_fraction(self):
        # X+Y+Z for X,Y,Z uniform [0,1]. CDF(1)=1/6;
        # after centering a side-.6 cube, plane height=-.3.
        self.assertAlmostEqual(self.fraction(normal=(1., 1., 1.), height=-.3), 1/6, places=12)
        self.assertAlmostEqual(self.fraction(normal=(1., 1., 1.), height=.3), 5/6, places=12)

    def test_oblique_planes_through_center_are_exactly_half(self):
        for normal in [(1., 1., 1.), (.2, 1., -.3), (1., 0., 0.), (0., 0., -2.)]:
            with self.subTest(normal=normal):
                self.assertAlmostEqual(self.fraction(normal=normal), .5, places=12)

    def test_rotation_translation_and_normal_scaling_covariance(self):
        q = (math.cos(.37), math.sin(.37)/math.sqrt(14),
             2*math.sin(.37)/math.sqrt(14), -3*math.sin(.37)/math.sqrt(14))
        ax = OBSERVER.rotation_axes(q)
        rotate = lambda v: tuple(sum(v[k]*ax[k][j] for k in range(3)) for j in range(3))
        center, normal, height = (.1, -.05, .02), (.2, 1., -.3), .04
        expected = self.fraction(center, normal, height)
        shift = (1.3, -2., .7)
        rc, rn = rotate(center), rotate(normal)
        actual = self.fraction(tuple(v+s for v, s in zip(rc, shift)), rn,
                               height+OBSERVER.dot(rn, shift), ax)
        self.assertAlmostEqual(actual, expected, places=12)
        self.assertAlmostEqual(self.fraction(center, tuple(v*4 for v in normal), height*4), expected, places=12)

    def test_complement_and_height_monotonicity(self):
        for normal in [(1., 1., 1.), (.2, 1., -.3), (.1, .3, .8)]:
            previous = -1
            for height in [-.6, -.3, -.15, 0., .15, .3, .6]:
                value = self.fraction(normal=normal, height=height)
                self.assertGreaterEqual(value, previous)
                self.assertAlmostEqual(value+self.fraction(normal=normal, height=-height), 1., places=12)
                previous = value

    def test_independent_dense_ray_clipping(self):
        # Independent vertical ray/halfspace integration: no CDF formula.
        # Fixed random seed changes the plane, never expected mass. Shape/frame
        # covariance has its own independent control above.
        rng = random.Random(743)
        for case in range(8):
            normal = tuple(rng.uniform(.25, 1.5) for _ in range(3))
            height = rng.uniform(-.25, .25)
            wet = 0.
            n = 192
            for iz in range(n):
                z = -.3 + (iz+.5)*.6/n
                for ix in range(n):
                    x = -.3 + (ix+.5)*.6/n
                    y = (height-normal[0]*x-normal[2]*z)/normal[1]
                    wet += max(0., min(.3, y)+.3)
            reference = wet/(n*n*.6)
            with self.subTest(case=case):
                self.assertAlmostEqual(self.fraction(normal=normal, height=height), reference, delta=2e-5)

    def test_degenerate_and_nonfinite_inputs_rejected(self):
        for half in [(0., .3, .3), (-1., .3, .3), (math.nan, .3, .3)]:
            with self.assertRaises(ValueError):
                OBSERVER.box_below_plane_fraction((0., 0., 0.), IDENTITY, half, (0., 1., 0.), 0.)
        for normal in [(0., 0., 0.), (0., math.inf, 0.)]:
            with self.assertRaises(ValueError):
                self.fraction(normal=normal)
        with self.assertRaises(ValueError):
            OBSERVER.rotation_axes((0., 0., 0., 0.))
        with self.assertRaises(ValueError):
            self.fraction(axes=((1., 1., 0.), (0., 1., 0.), (0., 0., 1.)))
        with self.assertRaises(ValueError):
            self.fraction(normal=(1e-12, 1., .3), height=.01)

    def test_no_exterior_data_is_not_dry_or_half(self):
        for particles in [[], [(0., 0., 0.)]]:
            with self.assertRaises(ValueError):
                OBSERVER.exterior_plane(particles, (0., 0., 0.), (.3, .3, .3), .25)
        with self.assertRaises(ValueError):
            OBSERVER.exterior_plane([(math.nan, 0., 0.)], (0., 0., 0.), (.3, .3, .3), .25)

    def test_legacy_upper_median_is_preserved(self):
        # Four exterior columns, heights 0,.1,.2,.3. The C++ legacy observer
        # chooses .2, not the interpolated statistical median .15.
        particles = [(x, h-.125, z) for (x, z), h in zip(
            [(-.875, -.875), (-.875, .875), (.875, -.875), (.875, .875)], [0., .1, .2, .3])]
        self.assertAlmostEqual(OBSERVER.exterior_plane(particles, (0., 0., 0.), (.3, .3, .3), .25)[0], .2)

    def test_remote_plane_fails_known_half_immersed_curved_surface(self):
        # h=.5*(x^2+z^2). Its exact mean over the box footprint is .03;
        # with COM at .03 all surfaces stay within the vertical box sides and
        # volume below the actual surface is exactly one half. Far-corner
        # columns do not measure that local surface.
        particles = []
        for x, z in itertools.product(range(8), repeat=2):
            px, pz = -1+(x+.5)*.25, -1+(z+.5)*.25
            particles.append((px, .5*(px*px+pz*pz)-.125, pz))
        remote, _, _, _ = OBSERVER.exterior_plane(particles, (0., .03, 0.), (.3, .3, .3), .25)
        reported = self.fraction(center=(0., .03, 0.), height=remote)
        self.assertEqual(reported, 1.)
        self.assertGreater(abs(reported-.5), .1)  # Negative control must remain detectable.

    def test_missing_solid_interior_needs_an_explicit_surface_continuation(self):
        # Geometric identifiability control, NOT two physical fluid solutions.
        # Exterior h=0 is compatible with either hypothetical interior cap
        # h=+/- .25*(1-(x/.3)^2)^2*(1-(z/.3)^2)^2. Both join h=0 with
        # zero first derivative on the footprint boundary. All observed fluid
        # outside the solid is identical; a continuation-defined volume is not.
        # Mean(1-u^2)^2 on [-1,1] is exactly 8/15. Both caps lie inside
        # the box's vertical extent, so their volume difference is analytic.
        delta = Fraction(1, 4)/Fraction(3, 5)*Fraction(8, 15)**2
        self.assertEqual(delta, Fraction(16, 135))
        fractions = (Fraction(1, 2)-delta, Fraction(1, 2)+delta)
        self.assertTrue(all(abs(value-Fraction(1, 2)) > Fraction(1, 10) for value in fractions))
        exterior = [(x, -.125, z) for x, z in itertools.product((-.875, .875), repeat=2)]
        self.assertEqual(OBSERVER.exterior_plane(exterior, (0., 0., 0.), (.3, .3, .3), .25)[0], 0.)
        self.assertEqual(self.fraction(), .5)  # This result chooses a flat cap.

    def make_replay_fixture(self, directory):
        rows = []
        body = "x,y,z,qw,qx,qy,qz,hx,hy,hz,gx,gy,gz\n0,.15,0,1,0,0,0,.3,.3,.3,0,-9.81,0\n"
        particles = "x,y,z\n-.875,-.125,-.875\n"
        for step in range(181, 241):
            rows.append(f"{step},original,.15,.15,.25,.25,0,1\n")
            for stage in ("before", "observe"):
                (directory / f"fixture_{stage}{step}_body.csv").write_text(body, encoding="utf-8")
                (directory / f"fixture_{stage}{step}_particles.csv").write_text(particles, encoding="utf-8")
        header = "step,window,begin_y,end_y,applied_fraction,independent_fraction,surface_y,fluid_updated\n"
        path = directory / "fixture_observations.csv"
        path.write_text(header+''.join(rows), encoding="utf-8")
        return path, header, rows

    def test_replay_retains_real_failure_instead_of_exit_zero_certification(self):
        with tempfile.TemporaryDirectory(prefix="judas-immersion-observer-") as temporary:
            directory = Path(temporary)
            self.make_replay_fixture(directory)
            report = OBSERVER.analyze(directory, "fixture", .25)
            window = report["windows"]["original"]
            self.assertEqual(window["samples"], 60)
            self.assertFalse(window["original_tolerance_pass"])
            self.assertAlmostEqual(window["analytic_plane_mean"], .25, places=12)

    def test_replay_rejects_missing_duplicate_invalid_and_mistimed_frames(self):
        with tempfile.TemporaryDirectory(prefix="judas-immersion-observer-") as temporary:
            directory = Path(temporary)
            path, header, rows = self.make_replay_fixture(directory)
            bad_cases = [rows[:-1], rows+[rows[0]],
                         [rows[0].replace(",.25,.25,0,1", ",.25,nan,0,1"), *rows[1:]],
                         [rows[0].replace(",original,.15,.15", ",original,.14,.15"), *rows[1:]]]
            for index, content in enumerate(bad_cases):
                with self.subTest(case=index):
                    path.write_text(header+''.join(content), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        OBSERVER.analyze(directory, "fixture", .25)
            path.write_text(header+''.join(rows), encoding="utf-8")
            # A window contract claiming late samples needs the actual frames.
            (directory / "fixture_settling.json").write_text(json.dumps(
                {"late_first_step": 480, "late_end_step": 720}), encoding="utf-8")
            with self.assertRaises(ValueError):
                OBSERVER.analyze(directory, "fixture", .25)


if __name__ == '__main__':
    unittest.main()
