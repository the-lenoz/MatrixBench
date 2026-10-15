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

if(BUILD_TESTING)
    add_executable(matrixbench_module_import_test
        tests/module_import_smoke.cpp
    )
    target_compile_features(matrixbench_module_import_test PRIVATE cxx_std_23)
    set_target_properties(matrixbench_module_import_test PROPERTIES
        CXX_EXTENSIONS OFF
    )
    target_link_libraries(matrixbench_module_import_test
        PRIVATE MatrixBench::matrixbench
    )

    add_test(
        NAME matrixbench.module_import
        COMMAND matrixbench_module_import_test
    )
endif()
