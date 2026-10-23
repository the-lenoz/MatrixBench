# MatrixBench specification

This file is the authoritative engineering specification for MatrixBench.
Keep it and `README.md` synchronized whenever public behavior, build
requirements, module names, or project scope changes.

## Goal and scope

MatrixBench is a C++23 library and CLI for comparing determinant algorithms and
square matrix memory layouts. It is not a general linear-algebra library. The
supported algorithms are Gaussian elimination and Bareiss; the supported
layouts are one contiguous allocation and separately allocated rows.

Do not add general matrix arithmetic, Coppersmith–Winograd, third-party
dependencies, STL containers, installation rules, packaging, or benchmarks
until the corresponding stage is explicitly requested. Tests may grow only
alongside behavior they can exercise.

Storage ownership, the matrix facade, Gaussian elimination, and Bareiss are
implemented. Changes to behavior must be introduced test-first and verified in
both layouts before this specification is updated.

## Project structure

Keep the production structure minimal:

```text
CMakeLists.txt
CMakePresets.json
README.md
AGENTS.md
LICENSE
src/storage.cppm
src/matrix.cppm
src/determinant.cppm
src/main.cpp
tests/matrixbench_tests.cpp
tests/cli_e2e.cmake
```

The explicitly requested benchmark/report extension adds only these
non-production artifacts:

```text
bench/benchmark_main.cpp
scripts/run_cli_benchmarks.py
scripts/check_benchmark_results.py
analysis/matrixbench_report.ipynb
```

`matrixbench_bench` is an opt-in executable, not a CTest benchmark. It writes
raw CSV samples and JSON metadata to a caller-supplied directory; generated
result directories are not versioned. Its core determinant timings include the
algorithm's mandatory working copy but exclude fixture construction. The CLI
script measures a separate end-to-end process experiment. Do not combine these
two timing layers in aggregate performance claims. Benchmark fixtures use a
fixed safe coefficient scale and the report is performance-only: numerical
accuracy and type-applicability boundaries belong to correctness tests, not to
the benchmark dataset.

Template implementations remain in the existing module interface units so
their definitions are reachable by importers. Do not create parallel headers
or implementation units merely to reproduce the same separation.

## Module `matrixbench.storage`

This module owns storage policies and their common concept. It exports all
names in namespace `matrixbench` and has no dependency on the matrix facade.

### `ContiguousStorage<T>`

Public interface:

```cpp
template<typename T>
class ContiguousStorage final {
public:
    using value_type = T;

    explicit ContiguousStorage(std::size_t size);
    ~ContiguousStorage();
    ContiguousStorage(const ContiguousStorage& other);
    ContiguousStorage& operator=(const ContiguousStorage& other);
    ContiguousStorage(ContiguousStorage&& other) noexcept;
    ContiguousStorage& operator=(ContiguousStorage&& other) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    T& operator()(std::size_t row, std::size_t column) noexcept;
    const T& operator()(std::size_t row, std::size_t column) const noexcept;
};
```

It owns one value-initialized allocation containing `size * size` elements.
Indexing uses `row * size + column`. Overflow in the logical element-count
calculation is detected before allocation and reported as `std::length_error`.

### `JaggedStorage<T>`

Its public interface is identical to `ContiguousStorage<T>`. It owns a `T**`
row table and one value-initialized `T[size]` allocation per row. Construction
must provide the strong exception guarantee by releasing any rows allocated
before a later allocation fails. It applies the same logical `size * size`
overflow check and `std::length_error` contract as contiguous storage.

### `MatrixStorage<Storage, T>`

The concept requires:

- `Storage::value_type` to be exactly `T`;
- copy construction and move support;
- `size() const noexcept` returning `std::size_t`;
- unchecked, `noexcept` mutable and const element access returning `T&` and
  `const T&` respectively.

Keeping this module independent allows storage ownership and layout to be
developed or verified without importing the higher-level matrix abstraction.

## Module `matrixbench.matrix`

This module publicly re-exports `matrixbench.storage` and owns only the square
matrix facade.

### `Matrix<T, StoragePolicy>`

Public interface:

```cpp
template<typename T, template<typename> typename StoragePolicy = ContiguousStorage>
    requires MatrixStorage<StoragePolicy<T>, T>
class Matrix final {
public:
    using value_type = T;
    using storage_type = StoragePolicy<T>;

    explicit Matrix(std::size_t size);
    ~Matrix();
    Matrix(const Matrix& other);
    Matrix& operator=(const Matrix& other);
    Matrix(Matrix&& other) noexcept;
    Matrix& operator=(Matrix&& other) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    T& operator()(std::size_t row, std::size_t column) noexcept;
    const T& operator()(std::size_t row, std::size_t column) const noexcept;
};
```

`Matrix` owns its storage policy and delegates size and element access to it.
It is always square. Size zero is valid, with no element accessible. Element
access performs no bounds check; both indices have the precondition
`index < size()`.

All three owning types follow the Rule of Five. Copy assignment uses an
independent temporary and provides the strong exception guarantee. After a
move, the source is valid, owns no allocation, and reports size zero.
Self-copy-assignment and self-move-assignment preserve the object. Destruction
and move operations are `noexcept`.

The policies are in a separate module from `Matrix` so storage layout can vary
without duplicating algorithms or coupling low-level ownership to the facade.

## Module `matrixbench.determinant`

This module publicly re-exports `matrixbench.matrix`, which in turn re-exports
`matrixbench.storage`, so clients normally need only:

```cpp
import matrixbench.determinant;
```

It exports these exact declarations:

```cpp
template<std::floating_point T, template<typename> typename Storage>
T determinant_gauss(
    const Matrix<T, Storage>& matrix,
    T epsilon = std::numeric_limits<T>::epsilon()
);

template<std::signed_integral T, template<typename> typename Storage>
T determinant_bareiss(const Matrix<T, Storage>& matrix);
```

Both functions treat their argument as immutable and operate on an independent
working copy. The determinant of a `0 x 0` matrix is `T{1}`.

### Gaussian elimination contract

`determinant_gauss` accepts floating-point element types. It uses partial
pivoting by column, changes the determinant sign after every row swap, reduces
the working copy to upper-triangular form, and multiplies the diagonal values.
A pivot with absolute value at most `epsilon` is treated as zero. `epsilon`
must be finite and nonnegative; invalid tolerance is reported with
`std::invalid_argument`. Inputs are expected to contain finite values.

### Bareiss contract

`determinant_bareiss` accepts signed integral element types. It performs
fraction-free elimination, uses row pivoting, preserves exact division, and
never converts values to floating point. It returns `T`. If a required
intermediate value or the exact determinant cannot be represented safely, it
throws `std::overflow_error` rather than invoking signed overflow or returning
a truncated result. Portable checked multiplication, subtraction, division,
and negation are used; the implementation does not depend on `__int128`.

Allocation failures from either algorithm propagate as `std::bad_alloc`.

Algorithms remain free functions rather than `Matrix` members so algorithm and
storage layout stay independent experimental dimensions.

## Command-line interface

The CMake target is `matrixbench_cli`; its output file is `matrixbench`. CLI
logic stays in `src/main.cpp` and imports `matrixbench.determinant`. Do not add
a CLI module or expose argument parsing through the library API.

Invocation requires exactly one value for each flag, in either order:

```text
matrixbench --storage contiguous|jagged --algorithm gauss|bareiss
```

`--help` is accepted only by itself and exits successfully. Other missing,
duplicate, unknown, or extra arguments produce exit code `2`.

The program reads a nonnegative `N` and exactly `N * N` row-major elements from
stdin, separated by arbitrary whitespace. Gauss reads finite `double` values;
Bareiss reads `long long` values. Missing, malformed, fractional Bareiss, or
trailing input produces exit code `3`. Size zero is valid.

Successful output is exactly the determinant followed by a newline, with no
stderr output. Gauss uses `std::numeric_limits<double>::max_digits10`; Bareiss
uses integer formatting. Allocation, arithmetic, output, and other computation
failures return `4`. Every error diagnostic goes to stderr and starts with
`matrixbench:`; error paths leave stdout empty.

## Language and dependency rules

- Use portable C++23 and disable compiler extensions.
- Support standards-conforming C++ module builds with both Clang and GCC; do
  not hard-code a compiler in CMake.
- Use CMake's `FILE_SET CXX_MODULES` and Ninja for the documented build.
- Keep `debug` and `release` configure, build, test, and workflow presets in
  `CMakePresets.json`. Presets select Ninja and enable tests but do not select a
  compiler; the caller or CLion toolchain owns that choice.
- Do not use `std::vector`, `std::array`, `std::deque`, `std::list`, associative
  containers, or any other STL container in project code.
- Non-container standard facilities are allowed, including concepts, type
  traits, exceptions, numeric limits, move utilities, algorithms, clocks,
  random-number facilities, and streams.
- Keep public names in namespace `matrixbench` and module names under the
  `matrixbench.` prefix.

## Verification

The test suite verifies both storage layouts; sizes `0`, `1`, and larger;
value initialization; deep copy; move-to-empty invariants; self-assignment;
construction and copy exception safety; singular, triangular, and row-swapping
matrices; preservation of the input; Gaussian tolerance behavior; exact
Bareiss results; and overflow reporting.

`tests/cli_e2e.cmake` is a process-level CTest harness. It generates input in
the build directory, launches `$<TARGET_FILE:matrixbench_cli>` without a shell,
and verifies exact exit code and stdout plus the required stderr diagnostic.
E2E coverage includes all four storage/algorithm combinations, fractional
Gauss input, strict input and flag failures, overflow, and `128 x 128` upper
triangular matrices. E2E tests carry the `e2e` label; the library test carries
the `unit` label.

Run both CMake workflows after behavior changes. Also verify fresh Clang and
GCC builds and run a Clang AddressSanitizer/UndefinedBehaviorSanitizer build for
ownership or arithmetic changes. Performance work must compare equivalent
inputs and keep setup outside measured regions. Benchmark reports use medians
and bootstrap confidence intervals over raw samples. Gauss (`double`) and
Bareiss (`long long`) are distinct numerical experiments; compare storage
layouts within an algorithm, not their absolute times across algorithms.
