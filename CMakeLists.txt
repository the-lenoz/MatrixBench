cmake_minimum_required(VERSION 3.28)

project(MatrixBench LANGUAGES CXX)

include(CTest)

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
    PRIVATE MatrixBench::matrixbench
)

add_executable(matrixbench_bench
    bench/benchmark_main.cpp
)
target_compile_features(matrixbench_bench PRIVATE cxx_std_23)
set_target_properties(matrixbench_bench PROPERTIES
    CXX_EXTENSIONS OFF
)
target_link_libraries(matrixbench_bench
    PRIVATE MatrixBench::matrixbench
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
    add_executable(matrixbench_tests
        tests/matrixbench_tests.cpp
    )
    target_compile_features(matrixbench_tests PRIVATE cxx_std_23)
    set_target_properties(matrixbench_tests PROPERTIES
        CXX_EXTENSIONS OFF
    )
    target_link_libraries(matrixbench_tests
        PRIVATE MatrixBench::matrixbench
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
endif()
