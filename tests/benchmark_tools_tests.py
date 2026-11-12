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

    def write_results(self, results):
        (self.directory / 'metadata.json').write_text(json.dumps(self.metadata))
        (self.directory / 'core.json').write_text(json.dumps(dict(
            context=dict(seed=str(self.metadata['seed'])), benchmarks=results)))

    def test_normalizes_ns(self):
        # Arrange
        self.write_results([self.result | dict(time_unit='ns')])

        # Act
        _, rows = load_core(self.directory)

        # Assert
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['time_ns'], 1)
        self.assertEqual(rows[0]['seed'], str(2**64-1))

    def test_normalizes_us(self):
        # Arrange
        self.write_results([self.result | dict(time_unit='us')])

        # Act
        _, rows = load_core(self.directory)

        # Assert
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['time_ns'], 1000)
        self.assertEqual(rows[0]['seed'], str(2**64-1))

    def test_normalizes_ms(self):
        # Arrange
        self.write_results([self.result | dict(time_unit='ms')])

        # Act
        _, rows = load_core(self.directory)

        # Assert
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['time_ns'], 1000000)
        self.assertEqual(rows[0]['seed'], str(2**64-1))

    def test_normalizes_s(self):
        # Arrange
        self.write_results([self.result | dict(time_unit='s')])

        # Act
        _, rows = load_core(self.directory)

        # Assert
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['time_ns'], 1000000000)
        self.assertEqual(rows[0]['seed'], str(2**64-1))

    def test_ignores_aggregate_rows(self):
        # Arrange
        self.write_results([self.result, dict(run_type='aggregate')])

        # Act
        _, rows = load_core(self.directory)

        # Assert
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['time_ns'], 1000)

    def test_rejects_negative_cpu_time(self):
        # Arrange
        self.write_results([self.result | dict(cpu_time=-1)])

        # Act / Assert
        with self.assertRaises(ValueError):
            load_core(self.directory)

    def test_rejects_nan_cpu_time(self):
        # Arrange
        self.write_results([self.result | dict(cpu_time=float('nan'))])

        # Act / Assert
        with self.assertRaises(ValueError):
            load_core(self.directory)

    def test_rejects_zero_iterations(self):
        # Arrange
        self.write_results([self.result | dict(iterations=0)])

        # Act / Assert
        with self.assertRaises(ValueError):
            load_core(self.directory)

    def test_rejects_errored_result(self):
        # Arrange
        self.write_results([self.result | dict(error_occurred=True)])

        # Act / Assert
        with self.assertRaises(ValueError):
            load_core(self.directory)

    def test_rejects_aggregate_only(self):
        # Arrange
        self.write_results([dict(run_type='aggregate')])

        # Act / Assert
        with self.assertRaises(ValueError):
            load_core(self.directory)

    def test_runner_core_forwarding_and_failure(self):
        # Arrange
        with patch.object(sys, 'argv', ['runner', '--experiment', 'core', '--bench', sys.executable,
                         '--output', str(self.directory), '--', '--benchmark_repetitions=2']), \
             patch.object(runner.subprocess, 'run') as run:
            run.return_value.returncode = 7
            # Act
            result = runner.main()

            # Assert
            self.assertEqual(result, 7)
            command = run.call_args.args[0]
            self.assertEqual(command[-1], '--benchmark_repetitions=2')
            self.assertIn('--benchmark_repetitions=5', command)

    def test_runner_cli_only(self):
        # Arrange
        with patch.object(sys, 'argv', ['runner', '--experiment', 'cli', '--cli', sys.executable,
                         '--output', str(self.directory)]), patch.object(runner, 'sizes', return_value=[1]), \
             patch.object(runner, 'run_case', return_value=(100, 0)) as run:
            # Act
            result = runner.main()

            # Assert
            self.assertEqual(result, 0)
            run.assert_called()
            self.assertTrue((self.directory / 'cli_samples.csv').is_file())

    def test_runner_warmup_failure(self):
        # Arrange
        with patch.object(sys, 'argv', ['runner', '--experiment', 'cli', '--cli', sys.executable,
                         '--output', str(self.directory)]), patch.object(runner, 'sizes', return_value=[1]), \
             patch.object(runner, 'run_case', return_value=(100, 4)):
            # Act
            result = runner.main()

            # Assert
            self.assertEqual(result, 2)

    def test_runner_both(self):
        # Arrange
        with patch.object(sys, 'argv', ['runner', '--bench', sys.executable, '--cli', sys.executable,
                         '--output', str(self.directory)]), patch.object(runner, 'sizes', return_value=[1]), \
             patch.object(runner.subprocess, 'run') as core, \
             patch.object(runner, 'run_case', return_value=(100, 0)) as cli:
            core.return_value.returncode = 0
            # Act
            result = runner.main()

            # Assert
            self.assertEqual(result, 0)
            core.assert_any_call([
                str(Path(sys.executable).resolve()), '--output', str(self.directory), '--mode', 'quick',
                '--seed', '20261023', '--filter', 'all', '--benchmark_repetitions=5',
            ], check=False)
            cli.assert_called()


if __name__ == '__main__':
    unittest.main()
