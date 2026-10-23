#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

import matrixbench.determinant;

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::string_view schema_version{"1"};

struct Options final {
    std::filesystem::path output_directory;
    std::uint64_t seed{20261023U};
    bool full{};
    std::string_view filter{"all"};
};

struct Case final {
    std::string_view algorithm;
    std::string_view family;
    std::string_view magnitude;
    std::size_t size{};
    std::uint64_t seed{};
};

volatile double double_sink{};
volatile long long integer_sink{};

[[noreturn]] void usage_error(std::string_view message) {
    throw std::invalid_argument{std::string{message}};
}

Options parse_options(int argc, char* argv[]) {
    Options options;
    bool has_output = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--output" && index + 1 < argc) {
            options.output_directory = argv[++index];
            has_output = true;
        } else if (argument == "--seed" && index + 1 < argc) {
            const char* value = argv[++index];
            char* end{};
            const auto parsed = std::strtoull(value, &end, 10);
            if (*value == '\0' || *end != '\0') {
                usage_error("--seed must be an unsigned integer");
            }
            options.seed = parsed;
        } else if (argument == "--mode" && index + 1 < argc) {
            const std::string_view value{argv[++index]};
            if (value == "full") {
                options.full = true;
            } else if (value != "quick") {
                usage_error("--mode must be quick or full");
            }
        } else if (argument == "--filter" && index + 1 < argc) {
            options.filter = argv[++index];
            if (options.filter != "all" && options.filter != "storage"
                && options.filter != "gauss" && options.filter != "bareiss") {
                usage_error("--filter must be all, storage, gauss, or bareiss");
            }
        } else if (argument == "--help") {
            std::cout << "Usage: matrixbench_bench --output DIR [--mode quick|full] "
                         "[--seed N] [--filter all|storage|gauss|bareiss]\n";
            std::exit(0);
        } else {
            usage_error("unknown or incomplete option");
        }
    }
    if (!has_output) {
        usage_error("--output is required");
    }
    return options;
}

const char* compiler_name() {
#if defined(__clang__)
    return "Clang " __clang_version__;
#elif defined(__GNUC__)
    return "GCC " __VERSION__;
#elif defined(_MSC_VER)
    return "MSVC";
#else
    return "unknown";
#endif
}

const char* operating_system() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "unknown";
#endif
}

void write_metadata(const Options& options) {
    std::ofstream metadata{options.output_directory / "metadata.json"};
    if (!metadata) {
        throw std::runtime_error{"cannot create metadata.json"};
    }
    metadata << "{\n"
             << "  \"schema_version\": \"" << schema_version << "\",\n"
             << "  \"git_revision\": \"" << MATRIXBENCH_GIT_REVISION << "\",\n"
             << "  \"git_dirty\": \"" << MATRIXBENCH_GIT_DIRTY << "\",\n"
             << "  \"compiler\": \"" << compiler_name() << "\",\n"
             << "  \"operating_system\": \"" << operating_system() << "\",\n"
             << "  \"hardware_threads\": " << std::thread::hardware_concurrency() << ",\n"
             << "  \"mode\": \"" << (options.full ? "full" : "quick") << "\",\n"
             << "  \"seed\": " << options.seed << ",\n"
             << "  \"filter\": \"" << options.filter << "\"\n"
             << "}\n";
}

template<template<typename> typename Storage>
matrixbench::Matrix<double, Storage> make_gauss_matrix(
    std::size_t size,
    std::string_view family,
    double scale,
    std::uint64_t seed
) {
    matrixbench::Matrix<double, Storage> matrix{size};
    std::mt19937_64 random{seed};
    std::uniform_real_distribution<double> coefficient{-0.25, 0.25};

    if (family == "singular") {
        for (std::size_t row = 0; row < size; ++row) {
            for (std::size_t column = 0; column < size; ++column) {
                matrix(row, column) = scale * (row == column ? 2.0 : coefficient(random));
            }
        }
        if (size > 1) {
            for (std::size_t column = 0; column < size; ++column) {
                matrix(size - 1, column) = matrix(0, column);
            }
        }
        return matrix;
    }

    if (family == "dense_pivot") {
        for (std::size_t row = 0; row < size; ++row) {
            for (std::size_t column = 0; column < size; ++column) {
                double value{};
                const std::size_t limit = row < column ? row : column;
                for (std::size_t index = 0; index <= limit; ++index) {
                    const double lower = row == index ? 1.0
                        : 0.05 * static_cast<double>(static_cast<long long>((row + index + seed) % 5) - 2);
                    const double upper = index == column
                        ? scale * (1.0 + (index % 7) * 0.01)
                        : scale * 0.05 * static_cast<double>(static_cast<long long>((index + column + seed) % 5) - 2);
                    value += lower * upper;
                }
                matrix(row, column) = value;
            }
        }
        if (size > 1) {
            for (std::size_t column = 0; column < size; ++column) {
                const double temporary = matrix(0, column);
                matrix(0, column) = matrix(1, column);
                matrix(1, column) = temporary;
            }
        }
        return matrix;
    }

    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t column = 0; column < size; ++column) {
            if (family == "diagonal") {
                matrix(row, column) = row == column ? scale * (1.0 + (row % 7) * 0.01) : 0.0;
            } else if (family == "upper_triangular") {
                matrix(row, column) = row > column ? 0.0
                    : (row == column ? scale * (1.0 + (row % 7) * 0.01) : scale * coefficient(random));
            } else {
                matrix(row, column) = scale * (row == column ? 2.0 : coefficient(random));
            }
        }
    }
    return matrix;
}

template<template<typename> typename Storage>
matrixbench::Matrix<long long, Storage> make_bareiss_matrix(
    std::size_t size,
    std::string_view family,
    long long range,
    std::uint64_t seed
) {
    matrixbench::Matrix<long long, Storage> matrix{size};
    std::mt19937_64 random{seed};
    std::uniform_int_distribution<long long> coefficient{-range, range};

    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t column = 0; column < size; ++column) {
            if (family == "diagonal") {
                matrix(row, column) = row == column ? (row % 2 == 0 ? 1 : -1) : 0;
            } else if (family == "upper_triangular") {
                matrix(row, column) = row > column ? 0 : (row == column ? 1 : coefficient(random));
            } else if (family == "singular") {
                matrix(row, column) = row == column ? 1 : 0;
            } else if (family == "dense_known") {
                long long value{};
                const std::size_t limit = row < column ? row : column;
                for (std::size_t index = 0; index <= limit; ++index) {
                    const long long lower = row == index ? 1
                        : ((row + index + seed) % 2 == 0 ? 1 : -1);
                    const long long upper = index == column ? 1
                        : static_cast<long long>((index * 17 + column * 31 + seed) %
                            static_cast<std::uint64_t>(2 * range + 1)) - range;
                    value += lower * upper;
                }
                matrix(row, column) = value;
            } else {
                matrix(row, column) = coefficient(random);
            }
        }
    }
    if (family == "singular" && size > 1) {
        for (std::size_t column = 0; column < size; ++column) {
            matrix(size - 1, column) = matrix(0, column);
        }
    }
    return matrix;
}

template<template<typename> typename Storage>
void benchmark_storage(std::ofstream& output, const Case& benchmark_case, const Options& options) {
    using Matrix = matrixbench::Matrix<double, Storage>;
    const unsigned int samples = options.full ? 30U : 3U;
    const auto target = options.full ? std::chrono::milliseconds{200} : std::chrono::milliseconds{5};

    auto write_phase = [&](std::string_view phase, auto&& operation) {
        unsigned int batch = 1;
        for (;;) {
            const auto start = Clock::now();
            for (unsigned int call = 0; call < batch; ++call) {
                operation();
            }
            if (Clock::now() - start >= target || batch >= 1U << 20U) {
                break;
            }
            batch *= 2;
        }
        for (unsigned int sample = 0; sample < samples; ++sample) {
            const auto start = Clock::now();
            for (unsigned int call = 0; call < batch; ++call) {
                operation();
            }
            const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
            output << schema_version << ',' << phase << ",storage,"
                   << (std::same_as<Storage<double>, matrixbench::ContiguousStorage<double>> ? "contiguous" : "jagged")
                   << ',' << benchmark_case.family << ',' << benchmark_case.size << ",1,"
                   << benchmark_case.seed << ',' << sample << ',' << batch
                   << ',' << static_cast<double>(nanoseconds) / batch << ",success\n";
        }
    };

    for (unsigned int warmup = 0; warmup < 3; ++warmup) {
        Matrix warm{benchmark_case.size};
        double_sink += static_cast<double>(warm.size());
    }
    write_phase("construct", [&] {
        Matrix matrix{benchmark_case.size};
        double_sink += static_cast<double>(matrix.size());
    });

    Matrix fixture{benchmark_case.size};
    write_phase("fill_row_major", [&] {
        for (std::size_t row = 0; row < fixture.size(); ++row) {
            for (std::size_t column = 0; column < fixture.size(); ++column) {
                fixture(row, column) = static_cast<double>(row + column);
            }
        }
        double_sink += fixture(0, 0);
    });
    write_phase("copy", [&] {
        Matrix copy{fixture};
        double_sink += static_cast<double>(copy.size());
    });
}

template<template<typename> typename Storage, typename Function>
void benchmark_algorithm(
    std::ofstream& output,
    const Case& benchmark_case,
    const Options& options,
    Function&& function
) {
    const unsigned int samples = options.full ? 30U : 3U;
    const auto target = options.full ? std::chrono::milliseconds{200} : std::chrono::milliseconds{5};
    for (unsigned int warmup = 0; warmup < 3; ++warmup) {
        try {
            function();
        } catch (const std::overflow_error&) {
        }
    }

    unsigned int batch = 1;
    for (;;) {
        const auto start = Clock::now();
        for (unsigned int call = 0; call < batch; ++call) {
            try {
                function();
            } catch (const std::overflow_error&) {
            }
        }
        if (Clock::now() - start >= target || batch >= 1U << 20U) {
            break;
        }
        batch *= 2;
    }

    for (unsigned int sample = 0; sample < samples; ++sample) {
        std::string_view status{"success"};
        const auto start = Clock::now();
        for (unsigned int call = 0; call < batch; ++call) {
            try {
                function();
            } catch (const std::overflow_error&) {
                status = "overflow";
            } catch (...) {
                status = "error";
            }
        }
        const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
        output << schema_version << ",determinant," << benchmark_case.algorithm << ','
               << (std::same_as<Storage<double>, matrixbench::ContiguousStorage<double>> ? "contiguous" : "jagged")
               << ',' << benchmark_case.family << ',' << benchmark_case.size << ',' << benchmark_case.magnitude
               << ',' << benchmark_case.seed << ',' << sample << ',' << batch << ','
               << static_cast<double>(nanoseconds) / batch << ',' << status << '\n';
    }
}

template<template<typename> typename Storage>
void run_gauss_case(
    std::ofstream& output,
    const Case& benchmark_case,
    const Options& options,
    double scale
) {
    const auto matrix = make_gauss_matrix<Storage>(
        benchmark_case.size, benchmark_case.family, scale, benchmark_case.seed
    );
    benchmark_algorithm<Storage>(output, benchmark_case, options, [&] {
        double_sink += matrixbench::determinant_gauss(matrix);
    });
}

template<template<typename> typename Storage>
void run_bareiss_case(
    std::ofstream& output,
    const Case& benchmark_case,
    const Options& options,
    long long range
) {
    const auto matrix = make_bareiss_matrix<Storage>(
        benchmark_case.size, benchmark_case.family, range, benchmark_case.seed
    );
    benchmark_algorithm<Storage>(output, benchmark_case, options, [&] {
        integer_sink += matrixbench::determinant_bareiss(matrix);
    });
}

void run_storage_cases(std::ofstream& output, const Options& options) {
    const std::size_t quick_sizes[] = {1, 8, 32, 128};
    const std::size_t full_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
    const std::size_t* sizes = options.full ? full_sizes : quick_sizes;
    const std::size_t count = options.full ? std::size(full_sizes) : std::size(quick_sizes);
    for (std::size_t index = 0; index < count; ++index) {
        const Case benchmark_case{"storage", "row_major", "1", sizes[index], options.seed};
        benchmark_storage<matrixbench::ContiguousStorage>(output, benchmark_case, options);
        benchmark_storage<matrixbench::JaggedStorage>(output, benchmark_case, options);
    }
}

void run_gauss_cases(std::ofstream& output, const Options& options) {
    const std::size_t quick_sizes[] = {1, 8, 32, 128};
    const std::size_t full_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
    const std::size_t* sizes = options.full ? full_sizes : quick_sizes;
    const std::size_t count = options.full ? std::size(full_sizes) : std::size(quick_sizes);
    const std::string_view families[] = {"diagonal", "upper_triangular", "dense_pivot", "singular"};
    for (std::size_t size_index = 0; size_index < count; ++size_index) {
        for (const std::string_view family : families) {
            const Case benchmark_case{"gauss", family, "1", sizes[size_index], options.seed};
            run_gauss_case<matrixbench::ContiguousStorage>(output, benchmark_case, options, 1.0);
            run_gauss_case<matrixbench::JaggedStorage>(output, benchmark_case, options, 1.0);
        }
    }
}

void run_bareiss_cases(std::ofstream& output, const Options& options) {
    const std::size_t quick_sizes[] = {1, 8, 32, 128};
    const std::size_t full_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
    const std::size_t* sizes = options.full ? full_sizes : quick_sizes;
    const std::size_t count = options.full ? std::size(full_sizes) : std::size(quick_sizes);
    const std::string_view families[] = {"diagonal", "upper_triangular", "dense_known", "singular"};
    for (std::size_t size_index = 0; size_index < count; ++size_index) {
        for (const std::string_view family : families) {
            const Case benchmark_case{"bareiss", family, "1", sizes[size_index], options.seed};
            run_bareiss_case<matrixbench::ContiguousStorage>(output, benchmark_case, options, 1);
            run_bareiss_case<matrixbench::JaggedStorage>(output, benchmark_case, options, 1);
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = parse_options(argc, argv);
        std::filesystem::create_directories(options.output_directory);
        write_metadata(options);
        std::ofstream output{options.output_directory / "core_samples.csv"};
        if (!output) {
            throw std::runtime_error{"cannot create core_samples.csv"};
        }
        output << "schema_version,phase,algorithm,storage,family,size,magnitude,seed,sample,batch,time_ns,status\n";
        if (options.filter == "all" || options.filter == "storage") {
            run_storage_cases(output, options);
        }
        if (options.filter == "all" || options.filter == "gauss") {
            run_gauss_cases(output, options);
        }
        if (options.filter == "all" || options.filter == "bareiss") {
            run_bareiss_cases(output, options);
        }
        return output ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "matrixbench_bench: " << error.what() << '\n';
        return 2;
    }
}
