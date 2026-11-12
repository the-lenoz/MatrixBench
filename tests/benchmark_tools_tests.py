"""Contract tests for benchmark result loading and the process runner."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from check_benchmark_results import load_core
import run_cli_benchmarks as runner


class ToolsTests(unittest.TestCase):
    def setUp(self):
        processor = patch.object(runner.platform, "processor", return_value="test CPU")
        processor.start()
        self.addCleanup(processor.stop)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.metadata = dict(schema_version='2', seed=2**64-1, output_format='json',
                             output_path=str(self.directory / 'core.json'))
        self.result = dict(name='determinant/gauss/jagged/diagonal/1', run_type='iteration',
                           iterations=2, cpu_time=1, real_time=2, time_unit='us')

    def load(self, results):
        (self.directory / 'metadata.json').write_text(json.dumps(self.metadata))
        (self.directory / 'core.json').write_text(json.dumps(dict(
            context=dict(seed=str(self.metadata['seed'])), benchmarks=results)))
        return load_core(self.directory)[1]

    def test_units_and_aggregates(self):
        for unit, factor in [('ns', 1), ('us', 1000), ('ms', 10**6), ('s', 10**9)]:
            self.result['time_unit'] = unit
            rows = self.load([self.result, dict(run_type='aggregate')])
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]['time_ns'], factor)
            self.assertEqual(rows[0]['seed'], str(2**64-1))

    def test_bad_results(self):
        for change in [dict(cpu_time=-1), dict(cpu_time=float('nan')),
                       dict(iterations=0), dict(error_occurred=True)]:
            with self.assertRaises(ValueError):
                self.load([self.result | change])
        with self.assertRaises(ValueError):
            self.load([dict(run_type='aggregate')])

    def test_runner_core_forwarding_and_failure(self):
        with patch.object(sys, 'argv', ['runner', '--experiment', 'core', '--bench', sys.executable,
                         '--output', str(self.directory), '--', '--benchmark_repetitions=2']), \
             patch.object(runner.subprocess, 'run') as run:
            run.return_value.returncode = 7
            self.assertEqual(runner.main(), 7)
            command = run.call_args.args[0]
            self.assertEqual(command[-1], '--benchmark_repetitions=2')
            self.assertIn('--benchmark_repetitions=5', command)

    def test_runner_cli_only(self):
        with patch.object(sys, 'argv', ['runner', '--experiment', 'cli', '--cli', sys.executable,
                         '--output', str(self.directory)]), patch.object(runner, 'sizes', return_value=[1]), \
             patch.object(runner, 'run_case', return_value=(100, 0)) as run:
            self.assertEqual(runner.main(), 0)
            self.assertTrue(run.called)
            self.assertTrue((self.directory / 'cli_samples.csv').is_file())

    def test_runner_warmup_failure(self):
        with patch.object(sys, 'argv', ['runner', '--experiment', 'cli', '--cli', sys.executable,
                         '--output', str(self.directory)]), patch.object(runner, 'sizes', return_value=[1]), \
             patch.object(runner, 'run_case', return_value=(100, 4)):
            self.assertEqual(runner.main(), 2)

    def test_runner_both(self):
        with patch.object(sys, 'argv', ['runner', '--bench', sys.executable, '--cli', sys.executable,
                         '--output', str(self.directory)]), patch.object(runner, 'sizes', return_value=[1]), \
             patch.object(runner.subprocess, 'run') as core, \
             patch.object(runner, 'run_case', return_value=(100, 0)) as cli:
            core.return_value.returncode = 0
            self.assertEqual(runner.main(), 0)
            self.assertTrue(core.called and cli.called)


if __name__ == '__main__':
    unittest.main()
