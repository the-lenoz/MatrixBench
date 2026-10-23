# MatrixBench

MatrixBench is a C++23 library and command-line program for studying how a
square matrix's memory layout and determinant algorithm affect performance.
Storage ownership, the matrix facade, Gaussian elimination, Bareiss, and the
CLI are implemented and covered by tests.

## Scope

The library defines two storage layouts:

- `ContiguousStorage<T>` stores all elements in one `T*` allocation;
- `JaggedStorage<T>` stores separately allocated rows through `T**`.

It provides determinant calculation by Gaussian elimination for
floating-point matrices and by the Bareiss algorithm for signed integral
matrices. General matrix arithmetic and the Coppersmith–Winograd algorithm are
intentionally outside the project scope.

STL containers such as `std::vector` and `std::array` are not used. Standard
library facilities that are not containers remain allowed.

Storage policies and their common concept live in the independent
`matrixbench.storage` module. The `matrixbench.matrix` module contains only the
`Matrix` facade and publicly re-exports storage, keeping memory layout separate
from the matrix abstraction.

## Requirements and build

- a C++23 compiler with C++ modules support;
- CMake 3.28 or newer;
- Ninja.

### CLion and CMake presets

Open the repository root in CLion and enable CMake presets if the IDE asks.
The project provides `debug` and `release` configure presets; both select Ninja
and enable tests. Matching build and test presets let CLion build either
configuration and run its tests from the corresponding toolbar actions.

Complete configure/build/test workflows are also available:

```sh
cmake --workflow --preset debug
cmake --workflow --preset release
```

The compiler is intentionally not fixed by the presets. CLion uses the
compiler from the selected toolchain.

Configure and build with Clang:

```sh
cmake -S . -B build-clang -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang
```

Or with GCC:

```sh
cmake -S . -B build-gcc -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build-gcc
```

CMake exposes the library as `matrixbench` and as the namespaced alias
`MatrixBench::matrixbench`. It also builds the `matrixbench_cli` target whose
output file is named `matrixbench`, plus the `matrixbench_bench` benchmark
runner.

The test suite covers both storage layouts, Rule of Five behavior, exception
safety, matrix delegation, both determinant algorithms, input preservation,
invalid Gaussian tolerances, and integer overflow reporting. CTest also runs
31 process-level E2E scenarios against the compiled CLI, including all four
storage/algorithm combinations and generated `128 x 128` matrices.

## Command-line usage

Select one storage layout and one determinant algorithm with mandatory flags:

```sh
./cmake-build/debug/matrixbench --storage contiguous --algorithm gauss
./cmake-build/debug/matrixbench --storage jagged --algorithm bareiss
```

The program reads `N` followed by exactly `N * N` row-major elements from
standard input. Spaces and line breaks are interchangeable. For example:

```sh
printf '2 1 0 0 1\n' |
  ./cmake-build/debug/matrixbench --storage contiguous --algorithm bareiss
```

Output:

```text
1
```

`gauss` reads `double` elements and prints with round-trip precision.
`bareiss` reads `long long` elements and rejects fractional input. Missing or
extra elements are errors. Use `matrixbench --help` for a short usage summary.

Exit codes:

| Code | Meaning |
|---:|---|
| `0` | Success or `--help` |
| `2` | Missing, duplicate, or unknown command-line option |
| `3` | Invalid, incomplete, or excessive matrix input |
| `4` | Allocation, arithmetic, output, or other computation failure |

On success, stdout contains only the determinant and a newline. Errors leave
stdout empty and write a diagnostic prefixed with `matrixbench:` to stderr.

## Intended use

Client code imports the determinant module, which also re-exports the matrix
and storage modules:

```cpp
import matrixbench.determinant;

int main() {
    matrixbench::Matrix<double> matrix{2};
    matrix(0, 0) = 1.0;
    matrix(0, 1) = 2.0;
    matrix(1, 0) = 3.0;
    matrix(1, 1) = 4.0;

    const double determinant = matrixbench::determinant_gauss(matrix);
}
```

Both storage layouts reject a logical `n * n` element-count overflow with
`std::length_error`. Gaussian elimination rejects a negative or non-finite
`epsilon` with `std::invalid_argument`. Bareiss throws `std::overflow_error`
when its exact result or a required intermediate value cannot be represented
by the matrix element type.

See `AGENTS.md` for the complete module contracts, invariants, design
rationale, and rules for future implementation.

## Воспроизводимые бенчмарки

Набор бенчмарков измеряет ядро библиотеки и командную программу как два
отдельных эксперимента. Время ядра включает обязательное создание рабочей
копии внутри алгоритма, но не включает подготовку исходной матрицы. Отдельная
серия для хранилищ измеряет создание, заполнение по строкам и глубокое
копирование. CLI-серия включает запуск процесса, разбор текста, ввод и вывод,
поэтому её значения нельзя напрямую сравнивать со временем ядра. Скоростные
сценарии используют фиксированный безопасный масштаб коэффициентов; проверки
точности и границ применимости остаются в функциональных тестах.

Соберите оптимизированную конфигурацию Clang и запустите короткий smoke-набор:

```sh
cmake -S . -B build-clang-release -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build build-clang-release --target matrixbench_bench matrixbench_cli
./build-clang-release/matrixbench_bench \
  --output benchmark-results/quick --mode quick
python3 scripts/run_cli_benchmarks.py \
  --cli ./build-clang-release/matrixbench \
  --output benchmark-results/quick --mode quick
python3 scripts/check_benchmark_results.py benchmark-results/quick
```

Режим `--mode full` проверяет размеры от `1` до `512` и может выполняться
десятки минут. В режиме `quick` используются три выборки и короткая калибровка;
в режиме `full` — 30 выборок и не менее 200 мс полезного времени на точку.
Фильтр `--filter storage|gauss|bareiss` запускает только одну серию ядра.
Runner принимает `--seed N` (по умолчанию `20261023`) и сохраняет все сырые
замеры в `core_samples.csv`, а сведения о сборке — в `metadata.json`.
CLI-скрипт создаёт `cli_samples.csv` и отдельные
метаданные. Каталоги результатов намеренно игнорируются Git.

Откройте `analysis/matrixbench_report.ipynb` и при необходимости поменяйте
`RESULTS_DIR`. Ноутбук использует только стандартную библиотеку Python, NumPy и
Matplotlib. Он показывает bootstrap-интервалы, нормированное масштабирование,
влияние раскладки памяти, распределение повторов, стоимость операций
хранилища и отдельный график CLI. Проверки численной точности и поиск границ
применимости типов данных в бенчмарки не входят. Гаусс (`double`) и Барейса
(`long long`) анализируются как разные численные эксперименты, а не как прямое
сравнение на одном типе данных.
