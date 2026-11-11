import math
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'benchmarks'))
from performance.metrics import percentile, summarize
from performance.timeline import merge_intervals, clock_mapping
from performance.comparison import compare
import copy


class WholeFrameMetricsTests(unittest.TestCase):
    def test_regression_gate_rejects_unmatched_or_incomplete_evidence(self):
        identity = dict(workload_sha256='content', project_sha256='project', controls={},
                        duration_seconds=20, resolution='1280x720',
                        graphics_policy='4.6', cpu_model='reference',
                        gpu_model='reference', gpu_driver='same', cap_hz=0, profile=True)
        old = [dict(identity=identity, summary=summarize([10.] * 1000)) for _ in range(3)]
        new = copy.deepcopy(old)
        new[0]['identity'] = dict(identity, resolution='640x360')
        with self.assertRaises(ValueError):
            compare(old, new)
        new = copy.deepcopy(old)
        new[0]['identity'] = dict(identity, project_sha256='a different game')
        with self.assertRaises(ValueError):
            compare(old, new)
        with self.assertRaises(ValueError):
            compare(old[:1], old)
        new = copy.deepcopy(old)
        new[1]['dropped_events'] = 1
        with self.assertRaises(ValueError):
            compare(old, new)
        new = copy.deepcopy(old)
        for run in new:
            run['summary'] = summarize([12.] * 1000)
        self.assertFalse(compare(old, new)['regression_pass'])
        result = compare(old, old)
        self.assertTrue(result['regression_pass'])
        self.assertFalse(result['candidate_p99_120fps'])
        self.assertTrue(result['candidate_p99_60fps'])
        self.assertFalse(result['human_accepted'])

    def test_percentiles_and_lows(self):
        values = [1.] * 990 + [10.] * 9 + [100.]
        result = summarize(values)
        self.assertEqual(result['p99_ms'], 1.)
        self.assertEqual(result['worst_ms'], 100.)
        self.assertAlmostEqual(result['one_percent_low_fps'], 1000 / 19)
        self.assertAlmostEqual(result['point_one_percent_low_fps'], 10.)
        self.assertAlmostEqual(result['average_fps'], 1000000 / sum(values))

    def test_consecutive_hitches_and_real_budget(self):
        result = summarize([1, 20, 25, 1, 60])
        hitch = result['hitches'][str(16.666666667)]
        self.assertEqual(hitch['frames'], 3)
        self.assertEqual(hitch['longest_sequence_frames'], 2)
        self.assertEqual(hitch['longest_sequence_ms'], 60)
        self.assertFalse(result['p99_60fps'])

    def test_overlap_and_independent_device_clock(self):
        self.assertEqual(merge_intervals([(0, 10), (2, 4), (8, 15), (20, 25)]), [(0, 15), (20, 25)])
        start = {'cpu_before_ns': 100, 'cpu_after_ns': 120, 'gpu_ns': 10**18}
        end = {'cpu_before_ns': 1100, 'cpu_after_ns': 1120, 'gpu_ns': 10**18 + 1000}
        convert, uncertainty = clock_mapping(start, end)
        self.assertEqual(convert(10**18 + 500), 610)
        self.assertEqual(uncertainty, 10)
        with self.assertRaises(ValueError):
            clock_mapping(end, start)

    def test_invalid_samples_do_not_manufacture_pass(self):
        for values in [[], [0], [-1], [math.nan], [math.inf]]:
            with self.assertRaises(ValueError):
                summarize(values)
        with self.assertRaises(ValueError):
            percentile([], .99)


if __name__ == '__main__':
    unittest.main()
