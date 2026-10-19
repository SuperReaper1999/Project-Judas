"""Synthetic measurement-accounting controls; no engine or FPS qualification."""
import contextlib
import csv
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('dense_measure', ROOT / 'scripts/physics_dense_measure.py')
dense = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dense)


def csv_file(path, rows):
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


class DenseMeasurementTests(unittest.TestCase):
    def test_repository_default_root_is_portable(self):
        self.assertEqual(dense.ROOT, ROOT)
        self.assertEqual(dense.SCENE, 'projects/physics_stress_lab/Scenes/room1024.judas')

    def snapshot(self, directory, cadence=60):
        archive = directory / 'source-inputs.tar.gz'
        data = (f'constexpr float kFixedTimestep = 1.0f / {cadence}.0f;\n'
                'constexpr unsigned kMaxPhysicsStepsPerFrame = 8;\n').encode()
        with tarfile.open(archive, 'w:gz') as output:
            info = tarfile.TarInfo('src/SimulationTiming.h')
            info.size = len(data)
            output.addfile(info, io.BytesIO(data))
        return {'source_archive': {'path': str(directory / 'old-location' / archive.name),
                                   'sha256': hashlib.sha256(archive.read_bytes()).hexdigest()}}

    def test_relocated_snapshot_uses_verified_sibling_archive(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            frozen = self.snapshot(directory)
            timing = dense.frozen_timing_contract(frozen, directory)
            self.assertEqual(timing['fixed_cadence_hz'], 60)
            self.assertEqual(timing['maximum_fixed_steps_per_frame'], 8)

    def test_changed_snapshot_archive_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            frozen = self.snapshot(directory)
            frozen['source_archive']['sha256'] = '0' * 64
            with self.assertRaisesRegex(RuntimeError, 'archive changed'):
                dense.frozen_timing_contract(frozen, directory)

    def test_changed_binary_or_fixture_is_rejected_before_launch(self):
        for changed in ['fixture', 'binary']:
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                snapshot = directory / 'snapshot'
                binary = snapshot / 'bin/judas_physics_stress_live_benchmark'
                binary.parent.mkdir(parents=True)
                binary.write_bytes(b'original binary')
                fixture = directory / dense.SCENE
                fixture.parent.mkdir(parents=True)
                fixture.write_bytes(b'original fixture')
                frozen = dict(fixture=dict(sha256=dense.sha(fixture)), artifacts={
                    'bin/judas_physics_stress_live_benchmark': dict(copy_sha256=dense.sha(binary))})
                (snapshot / 'FROZEN.json').write_text(json.dumps(frozen))
                (fixture if changed == 'fixture' else binary).write_bytes(b'changed')
                args = SimpleNamespace(snapshot=snapshot, root=directory)
                expected = 'fixture differs' if changed == 'fixture' else 'executable changed'
                with mock.patch.object(dense, 'frozen_timing_contract', return_value={}), \
                        mock.patch.object(dense, 'execute') as execute:
                    with self.assertRaisesRegex(RuntimeError, expected):
                        dense.run(args)
                    execute.assert_not_called()

    def test_different_physics_cadence_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            frozen = self.snapshot(directory, cadence=30)
            with self.assertRaisesRegex(RuntimeError, 'unchanged 60 Hz'):
                dense.frozen_timing_contract(frozen, directory)

    def capture(self, directory, mode='realtime', wall_ms=1000, steps=8,
                frame_steps=None, requested_steps=None, exit_code=0, timed_out=False):
        csv_file(directory / 'steps.csv', [dict(step=i + 1, fixed_ms=10, bodies=8,
                 nonfinite_fields=0, trajectory_digest=str(i)) for i in range(steps)])
        csv_file(directory / 'frames.csv', [dict(interval_ms=0, wall_ms=0, fixed_steps=0),
                 dict(interval_ms=wall_ms, wall_ms=wall_ms,
                      fixed_steps=steps if frame_steps is None else frame_steps)])
        (directory / 'quality.json').write_text('{}')
        entry = dict(exit=exit_code, timed_out=timed_out)
        args = SimpleNamespace(mode=mode, seconds=1,
                               steps=steps if requested_steps is None else requested_steps)
        dense.live_summary(directory, entry, args, {'maximum_fixed_steps_per_frame': 8})
        return entry

    def test_successful_early_quit_is_not_complete_realtime_capture(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.capture(Path(temporary), wall_ms=100)
            self.assertFalse(result['valid_completion'])
            self.assertFalse(result['realtime_requested_window_reached'])

    def test_slow_realtime_capture_exposes_simulation_progress_and_full_cap_count(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.capture(Path(temporary), wall_ms=2000)
            self.assertTrue(result['valid_completion'])
            self.assertEqual(result['catchup_limit_reached_frames_full_csv'], 1)
            self.assertLess(result['simulation_seconds_per_wall_second'], .1)
            self.assertEqual(result['rendered_cadence_observation']['mean_output_fps'], .5)
            self.assertIn('Capture completion only', result['acceptance_note'])

    def test_frame_step_accounting_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.capture(Path(temporary), frame_steps=7)
            self.assertFalse(result['valid_completion'])
            self.assertFalse(result['frame_and_step_csv_counts_agree'])

    def test_synthetic_completion_does_not_claim_fps(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.capture(Path(temporary), mode='exact')
            self.assertTrue(result['valid_completion'])
            self.assertIsNone(result['rendered_cadence_observation'])
            self.assertIn('NOT displayed FPS', result['cadence'])

    def test_short_or_failed_capture_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.assertFalse(self.capture(directory, mode='exact', requested_steps=9)['valid_completion'])
            self.assertFalse(self.capture(directory, exit_code=7)['valid_completion'])
            self.assertFalse(self.capture(directory, timed_out=True)['valid_completion'])

    def test_retained_profiler_history_is_deduplicated_and_bounded(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            first = dict(id=1, fixed=dict(cap_reached=True, discarded_seconds=.25))
            second = dict(id=2, fixed=dict(cap_reached=True, discarded_seconds=.5))
            (directory / 'profile-shutdown.json').write_text(json.dumps(
                dict(frames=[first], spikes=[first, second])))
            result = dense.profile_summary(directory)
            self.assertEqual(result['retained_unique_frames'], 2)
            self.assertEqual(result['cap_frames_in_retained_subset'], 2)
            self.assertEqual(result['discarded_seconds_in_retained_subset'], .75)
            self.assertIn('not an all-run cap counter', result['coverage'])


if __name__ == '__main__':
    unittest.main()
