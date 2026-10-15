# MatrixBench

MatrixBench is a C++23 library skeleton for studying how a square matrix's
memory layout and determinant algorithm affect performance. The project is
currently **interface-only**: its public contracts compile, but storage and
determinant functions are not implemented yet and therefore cannot be linked
when called.

## Scope

The library defines two storage layouts:

- `ContiguousStorage<T>` stores all elements in one `T*` allocation;
- `JaggedStorage<T>` stores separately allocated rows through `T**`.

It also declares determinant calculation by Gaussian elimination for
floating-point matrices and by the Bareiss algorithm for signed integral
matrices. General matrix arithmetic, a command-line application, and the
Coppersmith–Winograd algorithm are intentionally outside the project scope.

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
`MatrixBench::matrixbench`.

The current test is a smoke check that imports `matrixbench.determinant` and
verifies that both matrix layouts are reachable through the module re-export
chain. Algorithm tests will be added with their implementations.

## Intended use

Once implementations are added, client code will import the determinant
module, which also re-exports the matrix module:

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

This example documents the intended API only. At the current skeleton stage,
constructing the matrix or calling the algorithm produces unresolved symbols
at link time.

See `AGENTS.md` for the complete module contracts, invariants, design
rationale, and rules for future implementation.
