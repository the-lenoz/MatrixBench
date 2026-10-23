#include <cmath>
#include <concepts>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

import matrixbench.determinant;

namespace {

constexpr int usage_error_exit_code = 2;
constexpr int input_error_exit_code = 3;
constexpr int computation_error_exit_code = 4;

constexpr std::string_view usage =
    "Usage: matrixbench --storage <contiguous|jagged> --algorithm <gauss|bareiss>\n"
    "Reads N followed by exactly N*N matrix elements from standard input.\n"
    "gauss reads double; bareiss reads long long.\n";

class UsageError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class InputError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class StorageChoice {
    contiguous,
    jagged
};

enum class AlgorithmChoice {
    gauss,
    bareiss
};

struct Options final {
    StorageChoice storage{};
    AlgorithmChoice algorithm{};
    bool has_storage{};
    bool has_algorithm{};
};

Options parse_options(int argument_count, char* arguments[]) {
    Options options;

    for (int index = 1; index < argument_count; ++index) {
        const std::string_view argument{arguments[index]};

        if (argument == "--storage") {
            if (options.has_storage) {
                throw UsageError{"duplicate --storage"};
            }
            if (index + 1 >= argument_count) {
                throw UsageError{"missing value for --storage"};
            }

            const std::string_view value{arguments[++index]};
            if (value == "contiguous") {
                options.storage = StorageChoice::contiguous;
            } else if (value == "jagged") {
                options.storage = StorageChoice::jagged;
            } else {
                throw UsageError{"unknown storage"};
            }
            options.has_storage = true;
        } else if (argument == "--algorithm") {
            if (options.has_algorithm) {
                throw UsageError{"duplicate --algorithm"};
            }
            if (index + 1 >= argument_count) {
                throw UsageError{"missing value for --algorithm"};
            }

            const std::string_view value{arguments[++index]};
            if (value == "gauss") {
                options.algorithm = AlgorithmChoice::gauss;
            } else if (value == "bareiss") {
                options.algorithm = AlgorithmChoice::bareiss;
            } else {
                throw UsageError{"unknown algorithm"};
            }
            options.has_algorithm = true;
        } else if (argument == "--help") {
            throw UsageError{"--help must be used alone"};
        } else {
            throw UsageError{"unknown argument"};
        }
    }

    if (!options.has_storage) {
        throw UsageError{"missing required --storage"};
    }
    if (!options.has_algorithm) {
        throw UsageError{"missing required --algorithm"};
    }
    return options;
}

template<typename T, template<typename> typename Storage>
matrixbench::Matrix<T, Storage> read_matrix() {
    long long signed_size{};
    if (!(std::cin >> signed_size)) {
        throw InputError{"failed to read matrix size"};
    }
    if (signed_size < 0) {
        throw InputError{"matrix size must be nonnegative"};
    }
    if (!std::in_range<std::size_t>(signed_size)) {
        throw InputError{"matrix size is too large"};
    }

    matrixbench::Matrix<T, Storage> matrix{static_cast<std::size_t>(signed_size)};
    for (std::size_t row = 0; row < matrix.size(); ++row) {
        for (std::size_t column = 0; column < matrix.size(); ++column) {
            T value{};
            if (!(std::cin >> value)) {
                throw InputError{"failed to read matrix element"};
            }
            if constexpr (std::floating_point<T>) {
                if (!std::isfinite(value)) {
                    throw InputError{"matrix elements must be finite"};
                }
            }
            matrix(row, column) = value;
        }
    }

    std::cin >> std::ws;
    if (std::cin.peek() != std::char_traits<char>::eof()) {
        throw InputError{"unexpected trailing input"};
    }
    return matrix;
}

void verify_output() {
    if (!std::cout) {
        throw std::runtime_error{"failed to write output"};
    }
}

template<template<typename> typename Storage>
void run_gauss() {
    const auto matrix = read_matrix<double, Storage>();
    const double determinant = matrixbench::determinant_gauss(matrix);
    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10)
              << determinant << '\n';
    verify_output();
}

template<template<typename> typename Storage>
void run_bareiss() {
    const auto matrix = read_matrix<long long, Storage>();
    const long long determinant = matrixbench::determinant_bareiss(matrix);
    std::cout << determinant << '\n';
    verify_output();
}

void dispatch(const Options& options) {
    if (options.algorithm == AlgorithmChoice::gauss) {
        if (options.storage == StorageChoice::contiguous) {
            run_gauss<matrixbench::ContiguousStorage>();
        } else {
            run_gauss<matrixbench::JaggedStorage>();
        }
    } else if (options.storage == StorageChoice::contiguous) {
        run_bareiss<matrixbench::ContiguousStorage>();
    } else {
        run_bareiss<matrixbench::JaggedStorage>();
    }
}

int report_error(int exit_code, const std::exception& error) {
    std::cerr << "matrixbench: " << error.what() << '\n';
    return exit_code;
}

} // namespace

int main(int argument_count, char* arguments[]) {
    if (argument_count == 2 && std::string_view{arguments[1]} == "--help") {
        std::cout << usage;
        return std::cout ? 0 : computation_error_exit_code;
    }

    try {
        const Options options = parse_options(argument_count, arguments);
        dispatch(options);
        return 0;
    } catch (const UsageError& error) {
        return report_error(usage_error_exit_code, error);
    } catch (const InputError& error) {
        return report_error(input_error_exit_code, error);
    } catch (const std::exception& error) {
        return report_error(computation_error_exit_code, error);
    } catch (...) {
        std::cerr << "matrixbench: unknown computation error\n";
        return computation_error_exit_code;
    }
}
