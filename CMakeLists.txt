cmake_minimum_required(VERSION 3.28)

project(MatrixBench LANGUAGES CXX)

include(CTest)

include(FetchContent)
set(CLI11_MODULES ON CACHE BOOL "Build the official CLI11 named module" FORCE)
set(CLI11_PRECOMPILED OFF CACHE BOOL "Use the module's own implementation" FORCE)
set(CLI11_SINGLE_FILE OFF CACHE BOOL "Do not generate a CLI11 header" FORCE)
set(CLI11_BUILD_TESTS OFF CACHE BOOL "Do not build dependency tests" FORCE)
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "Do not build dependency examples" FORCE)
set(CLI11_BUILD_DOCS OFF CACHE BOOL "Do not build dependency documentation" FORCE)
set(CLI11_INSTALL OFF CACHE BOOL "Do not install CLI11" FORCE)
FetchContent_Declare(cli11
    URL https://codeload.github.com/CLIUtils/CLI11/tar.gz/refs/tags/v2.7.2
    URL_HASH SHA256=46eef3101da70852ec7af026e09d485ccee81813331c8c6052d39344443b83da
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(cli11)
if(NOT TARGET CLI11::Module)
    message(FATAL_ERROR "MatrixBench requires the official CLI11::Module target")
endif()
# Compile the BMI and its consumers in the same language mode, including builds
# configured without presets. Never fall back to the header-only target.
target_compile_features(CLI11_Module PUBLIC cxx_std_23)
set_target_properties(CLI11_Module PROPERTIES CXX_EXTENSIONS OFF)

add_library(matrixbench)
add_library(MatrixBench::matrixbench ALIAS matrixbench)

target_compile_features(matrixbench PUBLIC cxx_std_23)
set_target_properties(matrixbench PROPERTIES
    CXX_EXTENSIONS OFF
)

target_sources(matrixbench
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            src/storage.cppm
            src/matrix.cppm
            src/determinant.cppm
)

add_executable(matrixbench_cli
    src/main.cpp
)
target_compile_features(matrixbench_cli PRIVATE cxx_std_23)
set_target_properties(matrixbench_cli PROPERTIES
    CXX_EXTENSIONS OFF
    OUTPUT_NAME matrixbench
)
target_link_libraries(matrixbench_cli
    PRIVATE MatrixBench::matrixbench CLI11::Module
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_LIBPFM OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_PYTHON OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googlebenchmark
    URL https://codeload.github.com/google/benchmark/tar.gz/refs/tags/v1.10.0
    URL_HASH SHA256=acc5c7c0fbed855c4aaa2d7c3c36fb82b0c15c28f4e151e3f679c2fc0aa404af
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(matrixbench_bench EXCLUDE_FROM_ALL
    bench/benchmark_main.cpp
)
target_compile_features(matrixbench_bench PRIVATE cxx_std_23)
set_target_properties(matrixbench_bench PROPERTIES
    CXX_EXTENSIONS OFF
)
target_link_libraries(matrixbench_bench
    PRIVATE MatrixBench::matrixbench CLI11::Module benchmark::benchmark
)

find_package(Git QUIET)
set(matrixbench_git_revision "unknown")
set(matrixbench_git_dirty "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE matrixbench_git_revision
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" status --porcelain
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE matrixbench_git_status
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(matrixbench_git_status)
        set(matrixbench_git_dirty "true")
    else()
        set(matrixbench_git_dirty "false")
    endif()
endif()
target_compile_definitions(matrixbench_bench PRIVATE
    MATRIXBENCH_GIT_REVISION="${matrixbench_git_revision}"
    MATRIXBENCH_GIT_DIRTY="${matrixbench_git_dirty}"
)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME matrixbench.bench.tools
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark_tools_tests.py")
        set_tests_properties(matrixbench.bench.tools PROPERTIES LABELS unit)
    endif()
    add_executable(matrixbench_tests
        tests/matrixbench_tests.cpp
    )
    target_compile_features(matrixbench_tests PRIVATE cxx_std_23)
    set_target_properties(matrixbench_tests PROPERTIES
        CXX_EXTENSIONS OFF
    )
    target_link_libraries(matrixbench_tests
        PRIVATE MatrixBench::matrixbench GTest::gmock GTest::gtest_main
    )

    add_test(
        NAME matrixbench.tests
        COMMAND matrixbench_tests
    )
    set_tests_properties(matrixbench.tests PROPERTIES LABELS unit)

    function(add_matrixbench_cli_e2e_test name scenario storage algorithm)
        add_test(
            NAME "matrixbench.cli.${name}"
            COMMAND
                ${CMAKE_COMMAND}
                "-DCLI=$<TARGET_FILE:matrixbench_cli>"
                "-DCASE=${scenario}"
                "-DSTORAGE=${storage}"
                "-DALGORITHM=${algorithm}"
                "-DTEST_ID=${name}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_e2e.cmake"
        )
        set_tests_properties("matrixbench.cli.${name}" PROPERTIES LABELS e2e)
    endfunction()

    foreach(storage IN ITEMS contiguous jagged)
        foreach(algorithm IN ITEMS gauss bareiss)
            add_matrixbench_cli_e2e_test(
                "${storage}.${algorithm}.identity"
                identity "${storage}" "${algorithm}"
            )
            add_matrixbench_cli_e2e_test(
                "${storage}.${algorithm}.known"
                known "${storage}" "${algorithm}"
            )
            add_matrixbench_cli_e2e_test(
                "${storage}.${algorithm}.large"
                large "${storage}" "${algorithm}"
            )
        endforeach()

        add_matrixbench_cli_e2e_test(
            "${storage}.gauss.fractional"
            fractional "${storage}" gauss
        )
    endforeach()

    add_matrixbench_cli_e2e_test(zero.gauss zero contiguous gauss)
    add_matrixbench_cli_e2e_test(zero.bareiss zero jagged bareiss)
    add_matrixbench_cli_e2e_test(reverse-order reverse_order jagged gauss)
    add_matrixbench_cli_e2e_test(help help none none)

    add_matrixbench_cli_e2e_test(error.missing-storage missing_storage none gauss)
    add_matrixbench_cli_e2e_test(error.missing-algorithm missing_algorithm contiguous none)
    add_matrixbench_cli_e2e_test(error.unknown-storage unknown_storage none none)
    add_matrixbench_cli_e2e_test(error.unknown-algorithm unknown_algorithm none none)
    add_matrixbench_cli_e2e_test(error.duplicate-storage duplicate_storage none none)
    add_matrixbench_cli_e2e_test(error.unknown-flag unknown_flag none none)
    add_matrixbench_cli_e2e_test(error.extra-argument extra_argument none none)
    add_matrixbench_cli_e2e_test(error.negative-size negative_size contiguous gauss)
    add_matrixbench_cli_e2e_test(error.unreadable-size unreadable_size contiguous gauss)
    add_matrixbench_cli_e2e_test(error.missing-elements missing_elements contiguous gauss)
    add_matrixbench_cli_e2e_test(error.trailing-token trailing_token contiguous gauss)
    add_matrixbench_cli_e2e_test(error.fractional-bareiss fractional_bareiss contiguous bareiss)
    add_matrixbench_cli_e2e_test(error.bareiss-overflow bareiss_overflow contiguous bareiss)
    foreach(scenario IN ITEMS
        duplicate_algorithm duplicate_same missing_storage_value missing_algorithm_value
        empty_storage empty_algorithm help_first help_last help_unknown short_help
        equals terminator no_args
    )
        add_matrixbench_cli_e2e_test("error.${scenario}" "${scenario}" none none)
    endforeach()

    foreach(scenario IN ITEMS
        smoke list google_time google_unknown google_invalid help no_args missing_output empty_output
        missing_output_value missing_mode_value missing_filter_value missing_seed_value
        duplicate_output duplicate_mode duplicate_filter duplicate_seed
        invalid_mode invalid_filter negative_seed overflow_seed fractional_seed
        malformed_seed signed_seed empty_seed unknown_flag extra_argument
        short_help help_first help_last equals terminator seed_zero seed_max
    )
        add_test(
            NAME "matrixbench.bench.cli.${scenario}"
            COMMAND ${CMAKE_COMMAND}
                "-DCLI=$<TARGET_FILE:matrixbench_bench>"
                "-DCASE=bench_${scenario}"
                "-DTEST_ID=bench-${scenario}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_e2e.cmake"
        )
        set_tests_properties("matrixbench.bench.cli.${scenario}" PROPERTIES LABELS e2e)
    endforeach()
endif()
