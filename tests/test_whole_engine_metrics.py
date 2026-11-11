import math
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'benchmarks'))
from performance.metrics import percentile, summarize


class WholeFrameMetricsTests(unittest.TestCase):
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

    def test_invalid_samples_do_not_manufacture_pass(self):
        for values in [[], [0], [-1], [math.nan], [math.inf]]:
            with self.assertRaises(ValueError):
                summarize(values)
        with self.assertRaises(ValueError):
            percentile([], .99)


if __name__ == '__main__':
    unittest.main()
