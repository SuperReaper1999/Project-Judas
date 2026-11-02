import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('budget', Path(__file__).resolve().parents[1]/'tools/validation/audit_fluid_force_budget.py')
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)


class ForceBudget(unittest.TestCase):
    def fixture(self):
        before = dict(observedBody=dict(id=1, world=9, valid=True), observedMass=2.,
            fixedStepSeconds=.25, completedNativeFixedSteps=3, observedGravity=[0., -10., 0.],
            observedLinearVelocity=[0., 1., 0.])
        after = copy.deepcopy(before)
        after.update(completedNativeFixedSteps=4, observedLinearVelocity=[0., 1.5, 0.],
            couplingSettings=dict(restDensity=1000.), exteriorReactionNotApplied=[0., 999., 0.],
            lastBodyMeasurement=dict(buoyancyForce=[0., 26., 0.], dragForce=[0., -2., 0.],
                                     containedImpulse=[0., 0., 0.], displacedVolume=.002, fraction=.5))
        return before, after

    def test_newton_budget_and_unapplied_reaction(self):
        result = module.budget(*self.fixture())
        self.assertEqual(result['momentum_change'], 1.)
        self.assertEqual(result['unattributed_residual'], 0.)
        self.assertEqual(result['static_buoyancy'], 5.)
        self.assertEqual(result['fluid_acceleration_load'], 1.5)
        self.assertEqual(result['exterior_reaction_NOT_applied'], 999.)

    def test_wrong_force_is_observable(self):
        before, after = self.fixture(); after['lastBodyMeasurement']['buoyancyForce'][1] += 4
        self.assertEqual(module.budget(before, after)['unattributed_residual'], -1.)

    def test_rotated_gravity(self):
        before, after = self.fixture()
        for record in (before, after):
            for key in ('observedGravity', 'observedLinearVelocity'):
                record[key] = [record[key][1], 0., 0.]
        for key in ('buoyancyForce', 'dragForce'):
            after['lastBodyMeasurement'][key] = [after['lastBodyMeasurement'][key][1], 0., 0.]
        self.assertEqual(module.budget(before, after)['unattributed_residual'], 0.)

    def test_missing_interval_or_changed_identity_rejected(self):
        for key, value in [('completedNativeFixedSteps', 9), ('observedMass', 3),
                           ('observedBody', dict(id=1, world=10, valid=True))]:
            before, after = self.fixture(); after[key] = value
            with self.assertRaises(ValueError): module.budget(before, after)

    def test_invalid_body_or_nonfinite_data_rejected(self):
        before, after = self.fixture(); before['observedBody']['valid'] = False
        with self.assertRaises(ValueError): module.budget(before, after)
        before, after = self.fixture(); after['lastBodyMeasurement']['fraction'] = float('nan')
        with self.assertRaises(ValueError): module.budget(before, after)
        before, after = self.fixture(); before['observedLinearVelocity'].append(99.)
        with self.assertRaises(ValueError): module.budget(before, after)


if __name__ == '__main__': unittest.main()
