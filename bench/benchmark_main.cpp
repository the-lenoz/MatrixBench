#include <charconv>
#include <benchmark/benchmark.h>
#include <memory>
#include <cstdlib>
#include <regex>
#include <cctype>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

import cli11;
import matrixbench.determinant;

namespace {

constexpr std::string_view schema_version{"2"};

struct Options final {
    std::filesystem::path output_directory;
    std::uint64_t seed{20261023U};
    bool full{};
    std::string filter{"all"};
};

struct Case final {
    std::string_view algorithm;
    std::string_view family;
    std::string_view magnitude;
    std::size_t size{};
    std::uint64_t seed{};
};

bool case_failed{};

// False means CLI11 printed help; no files or measurements should follow.
bool parse_options(int argc, char* argv[], Options& options) {
    CLI::App app{"Write Google Benchmark results and metadata to DIR.", "matrixbench_bench"};
    app.set_help_flag("--help", "Show this help and exit (use alone)");
    app.option_defaults()->multi_option_policy(CLI::MultiOptionPolicy::Throw);

    std::string output;
    std::string mode{"quick"};
    app.add_option("--output", output, "Result directory (created if needed)")
        ->required()->check(CLI::Validator{
            [](std::string& value) -> std::string {
                return value.empty() ? "directory must not be empty" : "";
            }, "DIR"});
    app.add_option("--mode", mode, "Matrix size set")
        ->check(CLI::IsMember({"quick", "full"}))->capture_default_str();
    app.add_option("--filter", options.filter, "Run only the selected core series")
        ->check(CLI::IsMember({"all", "storage", "gauss", "bareiss"}))->capture_default_str();
    app.add_option_function<std::string>("--seed", [&options](const std::string& value) {
        // CLI11's integer conversion permits non-decimal bases. Our seeds are
        // strictly decimal, and from_chars checks the full uint64_t range.
        std::uint64_t seed{};
        const auto result = std::from_chars(value.data(), value.data() + value.size(), seed, 10);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
            throw CLI::ValidationError{"--seed", "must be a decimal integer in [0, 18446744073709551615]"};
        }
        options.seed = seed;
    }, "Unsigned decimal fixture seed")
        ->type_name("UINT64")->default_str(std::to_string(options.seed));

    // Preserve the separate-token option syntax, just as in the main CLI.
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--" || (argument.starts_with("--")
                                && argument.find('=') != std::string_view::npos)) {
            throw CLI::ValidationError{"use --option value without '=' or '--'"};
        }
    }
    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp&) {
        if (argc != 2) {
            throw CLI::ValidationError{"--help", "must be used alone"};
        }
        std::cout << app.help();
        benchmark::PrintDefaultHelp();
        return false;
    }
    options.output_directory = output;
    options.full = mode == "full";
    return true;
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

std::string json_string(std::string_view value) {
    std::string result{"\""};
    constexpr char hex[]{"0123456789abcdef"};
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
        else if (c < 32) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
        else result += static_cast<char>(c);
    }
    return result + '"';
}

void write_metadata(const Options& options, const std::string& path, const std::string& format, int argc, char* argv[]) {
    std::ofstream metadata{options.output_directory / "metadata.json"};
    if (!metadata) {
        throw std::runtime_error{"cannot create metadata.json"};
    }
    metadata << "{\n  \"arguments\": [";
    for (int i = 0; i < argc; ++i) {
        if (i) metadata << ',';
        metadata << json_string(argv[i]);
    }
    metadata << "],\n"
             << "  \"output_path\": " << json_string(std::filesystem::absolute(path).string()) << ",\n"
             << "  \"output_format\": " << json_string(format) << ",\n"
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
    metadata.close();
    if (!metadata) throw std::runtime_error{"cannot write metadata.json"};
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

std::string case_name(const Case& c, std::string_view phase, bool contiguous) {
    return std::string{phase} + "/" + std::string{c.algorithm} + "/"
        + (contiguous ? "contiguous" : "jagged") + "/" + std::string{c.family}
        + "/" + std::to_string(c.size);
}

template<template<typename> typename Storage, int Phase>
void register_storage(const Case& c) {
    using Matrix = matrixbench::Matrix<double, Storage>;
    constexpr bool contiguous = std::same_as<Storage<double>, matrixbench::ContiguousStorage<double>>;
    constexpr auto phase = Phase == 0 ? "construct" : Phase == 1 ? "fill_row_major" : "copy";
    benchmark::RegisterBenchmark(case_name(c, phase, contiguous).c_str(),
        [c](benchmark::State& state) {
            try {
                Matrix fixture{c.size};
                for (std::size_t r = 0; r < c.size; ++r)
                    for (std::size_t col = 0; col < c.size; ++col)
                        fixture(r, col) = static_cast<double>(r + col);
                for (auto _ : state) {
                    if constexpr (Phase == 0) {
                        Matrix matrix{c.size};
                        benchmark::DoNotOptimize(matrix);
                        benchmark::ClobberMemory();
                    } else if constexpr (Phase == 2) {
                        Matrix copy{fixture};
                        benchmark::DoNotOptimize(copy);
                        benchmark::ClobberMemory();
                    } else {
                        benchmark::DoNotOptimize(fixture);
                        for (std::size_t r = 0; r < c.size; ++r)
                            for (std::size_t col = 0; col < c.size; ++col)
                                fixture(r, col) = static_cast<double>(r + col);
                        benchmark::ClobberMemory();
                    }
                }
            } catch (const std::exception& error) {
                case_failed = true;
                state.SkipWithError(error.what());
            }
        });
}

template<template<typename> typename Storage>
void benchmark_storage(const Case& c) {
    register_storage<Storage, 0>(c);
    register_storage<Storage, 1>(c);
    register_storage<Storage, 2>(c);
}

template<template<typename> typename Storage, bool Gauss>
void register_algorithm(const Case& c) {
    constexpr bool contiguous = std::same_as<Storage<double>, matrixbench::ContiguousStorage<double>>;
    benchmark::RegisterBenchmark(case_name(c, "determinant", contiguous).c_str(),
        [c](benchmark::State& state) {
            try {
                const auto fixture = [c] {
                    if constexpr (Gauss) return make_gauss_matrix<Storage>(c.size, c.family, 1.0, c.seed);
                    else return make_bareiss_matrix<Storage>(c.size, c.family, 1, c.seed);
                }();
                for (auto _ : state) {
                    auto result = [&] {
                        if constexpr (Gauss) return matrixbench::determinant_gauss(fixture);
                        else return matrixbench::determinant_bareiss(fixture);
                    }();
                    benchmark::DoNotOptimize(result);
                }
            } catch (const std::exception& error) {
                case_failed = true;
                state.SkipWithError(error.what());
            }
        });
}

void run_storage_cases(const Options& options) {
    const std::size_t quick_sizes[] = {1, 8, 32, 128};
    const std::size_t full_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
    const std::size_t* sizes = options.full ? full_sizes : quick_sizes;
    const std::size_t count = options.full ? std::size(full_sizes) : std::size(quick_sizes);
    for (std::size_t index = 0; index < count; ++index) {
        const Case benchmark_case{"storage", "row_major", "1", sizes[index], options.seed};
        benchmark_storage<matrixbench::ContiguousStorage>(benchmark_case);
        benchmark_storage<matrixbench::JaggedStorage>(benchmark_case);
    }
}

void run_gauss_cases(const Options& options) {
    const std::size_t quick_sizes[] = {1, 8, 32, 128};
    const std::size_t full_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
    const std::size_t* sizes = options.full ? full_sizes : quick_sizes;
    const std::size_t count = options.full ? std::size(full_sizes) : std::size(quick_sizes);
    const std::string_view families[] = {"diagonal", "upper_triangular", "dense_pivot", "singular"};
    for (std::size_t size_index = 0; size_index < count; ++size_index) {
        for (const std::string_view family : families) {
            const Case benchmark_case{"gauss", family, "1", sizes[size_index], options.seed};
            register_algorithm<matrixbench::ContiguousStorage, true>(benchmark_case);
            register_algorithm<matrixbench::JaggedStorage, true>(benchmark_case);
        }
    }
}

void run_bareiss_cases(const Options& options) {
    const std::size_t quick_sizes[] = {1, 8, 32, 128};
    const std::size_t full_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
    const std::size_t* sizes = options.full ? full_sizes : quick_sizes;
    const std::size_t count = options.full ? std::size(full_sizes) : std::size(quick_sizes);
    const std::string_view families[] = {"diagonal", "upper_triangular", "dense_known", "singular"};
    for (std::size_t size_index = 0; size_index < count; ++size_index) {
        for (const std::string_view family : families) {
            const Case benchmark_case{"bareiss", family, "1", sizes[size_index], options.seed};
            register_algorithm<matrixbench::ContiguousStorage, false>(benchmark_case);
            register_algorithm<matrixbench::JaggedStorage, false>(benchmark_case);
        }
    }
}

} // namespace

class CheckedReporter final : public benchmark::BenchmarkReporter {
    benchmark::BenchmarkReporter& reporter_;
public:
    bool failed{};
    explicit CheckedReporter(benchmark::BenchmarkReporter& reporter) : reporter_{reporter} {}
    bool ReportContext(const Context& c) override {
        reporter_.SetOutputStream(&GetOutputStream());
        reporter_.SetErrorStream(&GetErrorStream());
        return reporter_.ReportContext(c);
    }
    void ReportRunsConfig(double time, bool explicit_time, benchmark::IterationCount iterations) override {
        reporter_.ReportRunsConfig(time, explicit_time, iterations);
    }
    void ReportRuns(const std::vector<Run>& runs) override { reporter_.ReportRuns(runs); }
    void Finalize() override {
        reporter_.Finalize();
        GetOutputStream().flush();
        failed = !GetOutputStream();
    }
};

bool truthy(std::string value) {
    if (value.size() == 1) {
        const auto c = static_cast<unsigned char>(value[0]);
        return std::isalnum(c) && c != '0' && c != 'f' && c != 'F' && c != 'n' && c != 'N';
    }
    for (auto& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value != "false" && value != "no" && value != "off";
}

void validate_min_time(std::string_view value) {
    if (value.empty()) return; // Upstream accepts an empty time as zero.
    if (value.back() == 'x') {
        value.remove_suffix(1);
        long iterations{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), iterations);
        if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && iterations > 0) return;
    } else {
        if (value.back() == 's') value.remove_suffix(1);
        double seconds{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seconds);
        if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && std::isfinite(seconds) && seconds >= 0) return;
    }
    throw std::invalid_argument{"invalid --benchmark_min_time: expected nonnegative seconds or positive iterations with x suffix"};
}

// Google invokes this callback on invalid flag values. Throw before its exit path.
void invalid_google_options() {
    throw std::invalid_argument{"invalid Google Benchmark options"};
}

int main(int argc, char* argv[]) {
    try {
        auto own = std::make_unique<char*[]>(static_cast<std::size_t>(argc) + 1);
        auto google = std::make_unique<char*[]>(static_cast<std::size_t>(argc) + 2);
        int own_count = 1, google_count = 1;
        own[0] = google[0] = argv[0];
        std::string output_path = std::getenv("BENCHMARK_OUT") ? std::getenv("BENCHMARK_OUT") : "";
        std::string output_format = std::getenv("BENCHMARK_OUT_FORMAT") ? std::getenv("BENCHMARK_OUT_FORMAT") : "json";
        std::string regex_filter = std::getenv("BENCHMARK_FILTER") ? std::getenv("BENCHMARK_FILTER") : ".*";
        std::string min_time = std::getenv("BENCHMARK_MIN_TIME") ? std::getenv("BENCHMARK_MIN_TIME") : "0.5s";
        bool list_only = std::getenv("BENCHMARK_LIST_TESTS") && truthy(std::getenv("BENCHMARK_LIST_TESTS"));
        for (int i = 1; i < argc; ++i) {
            const std::string_view token{argv[i]};
            if (token.starts_with("--benchmark_out=")) output_path = token.substr(16);
            else if (token.starts_with("--benchmark_out_format=")) output_format = token.substr(23);
            else if (token.starts_with("--benchmark_repetitions=")) {
                const auto value = token.substr(24);
                int repetitions{};
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), repetitions);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || repetitions < 1)
                    throw std::invalid_argument{"invalid --benchmark_repetitions"};
                google[google_count++] = argv[i];
            }
            else if (token.starts_with("--benchmark_") || token.starts_with("--v=")) {
                google[google_count++] = argv[i];
                if (token.starts_with("--benchmark_filter=")) regex_filter = token.substr(19);
                if (token.starts_with("--benchmark_min_time=")) min_time = token.substr(21);
                if (token == "--benchmark_list_tests") list_only = true;
                else if (token.starts_with("--benchmark_list_tests=")) list_only = truthy(std::string{token.substr(23)});
            } else {
                own[own_count++] = argv[i];
                if ((token == "--output" || token == "--mode" || token == "--seed" || token == "--filter") && i + 1 < argc)
                    own[own_count++] = argv[++i];
            }
        }
        for (int i = 1; i < argc; ++i)
            if (std::string_view{argv[i]} == "--help" && argc != 2)
                throw std::invalid_argument{"--help must be used alone"};
        Options options;
        if (!parse_options(own_count, own.get(), options)) return std::cout ? 0 : 1;
        validate_min_time(min_time);
        try { const std::regex checked_filter{regex_filter}; }
        catch (const std::regex_error&) { throw std::invalid_argument{"invalid --benchmark_filter regex"}; }
        if (output_format != "json" && output_format != "csv" && output_format != "console")
            throw std::invalid_argument{"invalid --benchmark_out_format"};
        if (output_path.empty()) output_path = (options.output_directory / "core_results.json").string();
        std::string out_option = "--benchmark_out=" + (list_only ? std::string{} : output_path);
        google[google_count++] = out_option.data();
        benchmark::Initialize(&google_count, google.get(), invalid_google_options);
        if (google_count != 1) throw std::invalid_argument{std::string{"unknown or invalid Google Benchmark option: "} + google[1]};
        if (options.filter == "all" || options.filter == "storage") run_storage_cases(options);
        if (options.filter == "all" || options.filter == "gauss") run_gauss_cases(options);
        if (options.filter == "all" || options.filter == "bareiss") run_bareiss_cases(options);
        if (list_only) {
            benchmark::RunSpecifiedBenchmarks();
            benchmark::Shutdown();
            return 0;
        }
        std::filesystem::create_directories(options.output_directory);
        std::ofstream output{output_path};
        if (!output) throw std::runtime_error{"cannot create benchmark output: " + output_path};
        benchmark::AddCustomContext("matrixbench_schema", "2");
        benchmark::AddCustomContext("seed", std::to_string(options.seed));
        benchmark::AddCustomContext("output_path", std::filesystem::absolute(output_path).string());
        benchmark::AddCustomContext("output_format", output_format);
        write_metadata(options, output_path, output_format, argc, argv);
        output.close();
        // Keep upstream display/file filtering separate, checking the file stream.
        std::unique_ptr<benchmark::BenchmarkReporter> reporter;
        if (output_format == "json") reporter = std::make_unique<benchmark::JSONReporter>();
        else if (output_format == "csv") reporter = std::make_unique<benchmark::CSVReporter>();
        else reporter = std::make_unique<benchmark::ConsoleReporter>();
        CheckedReporter checked{*reporter};
        auto count = benchmark::RunSpecifiedBenchmarks(nullptr, &checked);
        benchmark::Shutdown();
        if (checked.failed) { std::cerr << "matrixbench_bench: failed to write results\n"; return 1; }
        if (case_failed || count == 0) throw std::runtime_error{"failed or empty benchmark selection"};
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "matrixbench_bench: " << error.what() << '\n';
        return 2;
    }
}
