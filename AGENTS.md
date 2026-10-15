# MatrixBench specification

This file is the authoritative engineering specification for MatrixBench.
Keep it and `README.md` synchronized whenever public behavior, build
requirements, module names, or project scope changes.

## Goal and scope

MatrixBench is a C++23 library for comparing determinant algorithms and square
matrix memory layouts. It is not a general linear-algebra library. The supported
algorithms are Gaussian elimination and Bareiss; the supported layouts are one
contiguous allocation and separately allocated rows.

Do not add general matrix arithmetic, a CLI, Coppersmith–Winograd, third-party
dependencies, STL containers, installation rules, packaging, or benchmarks
until the corresponding stage is explicitly requested. Tests may grow only
alongside behavior they can exercise.

The current stage is an interface-only skeleton. Public declarations must
compile, but constructors, ownership operations, element access, and
determinant algorithms intentionally have no definitions. Calling them before
the implementation stage is expected to fail at link time.

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
tests/module_import_smoke.cpp
```

Template implementations must later be added to the existing module interface
units so their definitions are reachable by importers. Do not create parallel
headers or implementation units merely to reproduce the same separation.

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
Indexing uses `row * size + column`. Overflow in the element-count calculation
must be detected before allocation.

### `JaggedStorage<T>`

Its public interface is identical to `ContiguousStorage<T>`. It owns a `T**`
row table and one value-initialized `T[size]` allocation per row. Construction
must provide the strong exception guarantee by releasing any rows allocated
before a later allocation fails.

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

All three owning types follow the Rule of Five. Copying creates independent
storage and provides the strong exception guarantee. After a move, the source
is valid, owns no allocation, and reports size zero. Self-copy-assignment and
self-move-assignment are safe. Destruction and move operations are `noexcept`.

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
a truncated result.

Allocation failures from either algorithm propagate as `std::bad_alloc`.

Algorithms remain free functions rather than `Matrix` members so algorithm and
storage layout stay independent experimental dimensions.

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

## Future verification

When implementation is requested, add verification for both storage layouts;
sizes `0`, `1`, and larger; value initialization; deep copy; move-to-empty
invariants; self-assignment; singular, triangular, and row-swapping matrices;
preservation of the input; Gaussian tolerance behavior; exact Bareiss results;
and overflow reporting. Performance work must compare equivalent inputs and
keep setup outside measured regions.

The interface-only stage contains one executable CTest smoke check. It imports
`matrixbench.determinant` and confirms that both matrix layouts are reachable
through the re-export chain without invoking undefined operations. Add
algorithm and ownership tests only when their implementations exist.
