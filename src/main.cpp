#include <cmath>
#include <concepts>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

import cli11;
import matrixbench.determinant;

namespace {

constexpr int usage_error_exit_code = 2;
constexpr int input_error_exit_code = 3;
constexpr int computation_error_exit_code = 4;

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
};

// False means CLI11 printed help; no input or computation should follow.
bool parse_options(int argument_count, char* arguments[], Options& options) {
    CLI::App app{
        "Reads N followed by exactly N*N row-major matrix elements from stdin. "
        "gauss reads finite double values; bareiss reads long long values.",
        "matrixbench"
    };
    app.set_help_flag("--help", "Show this help and exit (use alone)");
    app.option_defaults()->multi_option_policy(CLI::MultiOptionPolicy::Throw);

    std::string storage;
    std::string algorithm;
    app.add_option("--storage", storage, "Square matrix memory layout")
        ->required()->check(CLI::IsMember({"contiguous", "jagged"}));
    app.add_option("--algorithm", algorithm, "Determinant algorithm")
        ->required()->check(CLI::IsMember({"gauss", "bareiss"}));

    // CLI11 accepts additional spellings by default. Keep our separate-token
    // interface; CLI11 handles values, required options, and duplicates.
    for (int index = 1; index < argument_count; ++index) {
        const std::string_view argument{arguments[index]};
        if (argument == "--" || (argument.starts_with("--")
                                && argument.find('=') != std::string_view::npos)) {
            throw CLI::ValidationError{"use --option value without '=' or '--'"};
        }
    }
    try {
        app.parse(argument_count, arguments);
    } catch (const CLI::CallForHelp&) {
        if (argument_count != 2) {
            throw CLI::ValidationError{"--help", "must be used alone"};
        }
        std::cout << app.help();
        return false;
    }
    options.storage = storage == "contiguous" ? StorageChoice::contiguous : StorageChoice::jagged;
    options.algorithm = algorithm == "gauss" ? AlgorithmChoice::gauss : AlgorithmChoice::bareiss;
    return true;
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
    try {
        Options options;
        if (!parse_options(argument_count, arguments, options)) {
            verify_output();
            return 0;
        }
        dispatch(options);
        return 0;
    } catch (const CLI::ParseError& error) {
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
