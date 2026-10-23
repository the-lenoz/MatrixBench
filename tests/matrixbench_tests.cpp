#include <cmath>
#include <concepts>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

import matrixbench.determinant;

namespace {

int failures = 0;

void fail(const char* expression, const char* file, int line) {
    ++failures;
    std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
}

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fail(#expression, __FILE__, __LINE__); \
        } \
    } while (false)

template<typename Exception, typename Function>
void check_throws(Function&& function, const char* expression, const char* file, int line) {
    try {
        function();
    } catch (const Exception&) {
        return;
    } catch (...) {
        fail(expression, file, line);
        return;
    }

    fail(expression, file, line);
}

#define CHECK_THROWS_AS(expression, exception_type) \
    check_throws<exception_type>([&] { static_cast<void>(expression); }, \
                                 #expression " throws " #exception_type, \
                                 __FILE__, \
                                 __LINE__)

template<typename Function>
void run_test(const char* name, Function&& function) {
    try {
        function();
    } catch (const std::exception& error) {
        ++failures;
        std::cerr << name << ": unexpected exception: " << error.what() << '\n';
    } catch (...) {
        ++failures;
        std::cerr << name << ": unexpected non-standard exception\n";
    }
}

bool approximately_equal(double left, double right, double tolerance = 1.0e-10) {
    return std::abs(left - right) <= tolerance;
}

template<typename T>
void self_copy_assign(T& value) {
    const T* alias = &value;
    value = *alias;
}

struct TestFailure final {};

struct TrackedValue final {
    static inline int alive = 0;
    static inline int default_budget = -1;
    static inline int assignment_budget = -1;

    int value{};

    TrackedValue() {
        if (default_budget == 0) {
            throw TestFailure{};
        }
        if (default_budget > 0) {
            --default_budget;
        }
        ++alive;
    }

    TrackedValue(const TrackedValue& other) : value(other.value) {
        ++alive;
    }

    TrackedValue& operator=(const TrackedValue& other) {
        if (assignment_budget == 0) {
            throw TestFailure{};
        }
        if (assignment_budget > 0) {
            --assignment_budget;
        }
        value = other.value;
        return *this;
    }

    ~TrackedValue() {
        --alive;
    }

    static void reset() noexcept {
        default_budget = -1;
        assignment_budget = -1;
    }
};

static_assert(matrixbench::MatrixStorage<matrixbench::ContiguousStorage<int>, int>);
static_assert(matrixbench::MatrixStorage<matrixbench::JaggedStorage<int>, int>);
static_assert(std::same_as<matrixbench::Matrix<int>::value_type, int>);
static_assert(std::same_as<matrixbench::Matrix<int>::storage_type,
                           matrixbench::ContiguousStorage<int>>);

template<template<typename> typename Storage>
void test_storage_basics() {
    Storage<int> empty{0};
    CHECK(empty.size() == 0);

    Storage<int> single{1};
    CHECK(single.size() == 1);
    CHECK(single(0, 0) == 0);

    Storage<int> storage{3};
    CHECK(storage.size() == 3);
    for (std::size_t row = 0; row < storage.size(); ++row) {
        for (std::size_t column = 0; column < storage.size(); ++column) {
            CHECK(storage(row, column) == 0);
            storage(row, column) = static_cast<int>(row * 10 + column);
        }
    }

    const Storage<int>& const_storage = storage;
    CHECK(const_storage(2, 1) == 21);

    Storage<int> copied{storage};
    storage(2, 1) = -1;
    CHECK(copied(2, 1) == 21);

    Storage<int> assigned{1};
    assigned(0, 0) = 99;
    assigned = copied;
    copied(0, 0) = -2;
    CHECK(assigned.size() == 3);
    CHECK(assigned(0, 0) == 0);

    self_copy_assign(assigned);
    CHECK(assigned.size() == 3);
    CHECK(assigned(2, 1) == 21);

    Storage<int> moved{std::move(assigned)};
    CHECK(assigned.size() == 0);
    CHECK(moved.size() == 3);
    CHECK(moved(2, 1) == 21);

    Storage<int> move_assigned{1};
    move_assigned = std::move(moved);
    CHECK(moved.size() == 0);
    CHECK(move_assigned.size() == 3);
    CHECK(move_assigned(2, 1) == 21);

    move_assigned = std::move(move_assigned);
    CHECK(move_assigned.size() == 3);
    CHECK(move_assigned(2, 1) == 21);

    CHECK_THROWS_AS(Storage<int>{std::numeric_limits<std::size_t>::max()},
                    std::length_error);
}

void test_jagged_partial_construction_cleanup() {
    CHECK(TrackedValue::alive == 0);
    TrackedValue::reset();
    TrackedValue::default_budget = 5;
    CHECK_THROWS_AS(matrixbench::JaggedStorage<TrackedValue>{3}, TestFailure);
    CHECK(TrackedValue::alive == 0);
    TrackedValue::reset();
}

template<template<typename> typename Storage>
void test_copy_assignment_strong_guarantee() {
    CHECK(TrackedValue::alive == 0);
    TrackedValue::reset();

    {
        Storage<TrackedValue> source{2};
        source(0, 0).value = 1;
        source(0, 1).value = 2;
        source(1, 0).value = 3;
        source(1, 1).value = 4;

        Storage<TrackedValue> destination{1};
        destination(0, 0).value = 99;

        TrackedValue::assignment_budget = 1;
        CHECK_THROWS_AS(destination = source, TestFailure);
        CHECK(destination.size() == 1);
        CHECK(destination(0, 0).value == 99);
        TrackedValue::reset();
    }

    CHECK(TrackedValue::alive == 0);
}

template<template<typename> typename Storage>
void test_matrix_facade() {
    matrixbench::Matrix<int, Storage> matrix{2};
    CHECK(matrix.size() == 2);
    CHECK(matrix(0, 0) == 0);
    matrix(0, 1) = 7;
    matrix(1, 0) = -3;

    const auto& const_matrix = matrix;
    CHECK(const_matrix(0, 1) == 7);
    CHECK(const_matrix(1, 0) == -3);

    auto copied = matrix;
    matrix(0, 1) = 10;
    CHECK(copied(0, 1) == 7);

    matrixbench::Matrix<int, Storage> assigned{1};
    assigned = copied;
    CHECK(assigned.size() == 2);
    CHECK(assigned(0, 1) == 7);
    self_copy_assign(assigned);
    CHECK(assigned(0, 1) == 7);

    auto moved = std::move(copied);
    CHECK(copied.size() == 0);
    CHECK(moved.size() == 2);
    CHECK(moved(0, 1) == 7);

    matrixbench::Matrix<int, Storage> move_assigned{1};
    move_assigned = std::move(moved);
    CHECK(moved.size() == 0);
    CHECK(move_assigned.size() == 2);
    CHECK(move_assigned(0, 1) == 7);
    move_assigned = std::move(move_assigned);
    CHECK(move_assigned.size() == 2);
    CHECK(move_assigned(0, 1) == 7);
}

template<template<typename> typename Storage>
void test_gauss() {
    matrixbench::Matrix<double, Storage> empty{0};
    CHECK(matrixbench::determinant_gauss(empty) == 1.0);

    matrixbench::Matrix<double, Storage> single{1};
    single(0, 0) = -3.5;
    CHECK(matrixbench::determinant_gauss(single) == -3.5);

    matrixbench::Matrix<double, Storage> matrix{2};
    matrix(0, 0) = 1.0;
    matrix(0, 1) = 2.0;
    matrix(1, 0) = 3.0;
    matrix(1, 1) = 4.0;
    CHECK(approximately_equal(matrixbench::determinant_gauss(matrix), -2.0));
    CHECK(matrix(0, 0) == 1.0);
    CHECK(matrix(0, 1) == 2.0);
    CHECK(matrix(1, 0) == 3.0);
    CHECK(matrix(1, 1) == 4.0);

    matrixbench::Matrix<double, Storage> row_swap{2};
    row_swap(0, 0) = 0.0;
    row_swap(0, 1) = 2.0;
    row_swap(1, 0) = 3.0;
    row_swap(1, 1) = 4.0;
    CHECK(approximately_equal(matrixbench::determinant_gauss(row_swap), -6.0));

    matrixbench::Matrix<double, Storage> triangular{3};
    triangular(0, 0) = 2.0;
    triangular(0, 1) = 8.0;
    triangular(0, 2) = -1.0;
    triangular(1, 1) = 3.0;
    triangular(1, 2) = 5.0;
    triangular(2, 2) = 4.0;
    CHECK(approximately_equal(matrixbench::determinant_gauss(triangular), 24.0));

    matrixbench::Matrix<double, Storage> singular{2};
    singular(0, 0) = 1.0;
    singular(0, 1) = 2.0;
    singular(1, 0) = 2.0;
    singular(1, 1) = 4.0;
    CHECK(matrixbench::determinant_gauss(singular) == 0.0);

    matrixbench::Matrix<double, Storage> near_zero{2};
    near_zero(0, 0) = 1.0e-8;
    near_zero(1, 1) = 1.0;
    CHECK(matrixbench::determinant_gauss(near_zero, 1.0e-7) == 0.0);
    CHECK(approximately_equal(matrixbench::determinant_gauss(near_zero, 1.0e-10),
                              1.0e-8,
                              1.0e-15));

    CHECK_THROWS_AS(matrixbench::determinant_gauss(matrix, -1.0),
                    std::invalid_argument);
    CHECK_THROWS_AS(
        matrixbench::determinant_gauss(matrix,
                                       std::numeric_limits<double>::infinity()),
        std::invalid_argument);
    CHECK_THROWS_AS(
        matrixbench::determinant_gauss(matrix,
                                       std::numeric_limits<double>::quiet_NaN()),
        std::invalid_argument);
}

template<template<typename> typename Storage>
void test_bareiss() {
    using Integer = long long;

    matrixbench::Matrix<Integer, Storage> empty{0};
    CHECK(matrixbench::determinant_bareiss(empty) == 1);

    matrixbench::Matrix<Integer, Storage> single{1};
    single(0, 0) = -7;
    CHECK(matrixbench::determinant_bareiss(single) == -7);

    matrixbench::Matrix<Integer, Storage> matrix{2};
    matrix(0, 0) = 1;
    matrix(0, 1) = 2;
    matrix(1, 0) = 3;
    matrix(1, 1) = 4;
    CHECK(matrixbench::determinant_bareiss(matrix) == -2);
    CHECK(matrix(0, 0) == 1);
    CHECK(matrix(0, 1) == 2);
    CHECK(matrix(1, 0) == 3);
    CHECK(matrix(1, 1) == 4);

    matrixbench::Matrix<Integer, Storage> row_swap{2};
    row_swap(0, 0) = 0;
    row_swap(0, 1) = 2;
    row_swap(1, 0) = 3;
    row_swap(1, 1) = 4;
    CHECK(matrixbench::determinant_bareiss(row_swap) == -6);

    matrixbench::Matrix<Integer, Storage> diagonal{3};
    diagonal(0, 0) = 2;
    diagonal(1, 1) = -3;
    diagonal(2, 2) = 4;
    CHECK(matrixbench::determinant_bareiss(diagonal) == -24);

    matrixbench::Matrix<Integer, Storage> singular{2};
    singular(0, 0) = 1;
    singular(0, 1) = 2;
    singular(1, 0) = 2;
    singular(1, 1) = 4;
    CHECK(matrixbench::determinant_bareiss(singular) == 0);

    matrixbench::Matrix<Integer, Storage> known{3};
    known(0, 0) = 2;
    known(0, 1) = 5;
    known(0, 2) = 3;
    known(1, 0) = 1;
    known(1, 1) = -2;
    known(1, 2) = -1;
    known(2, 0) = 1;
    known(2, 1) = 3;
    known(2, 2) = 4;
    CHECK(matrixbench::determinant_bareiss(known) == -20);

    matrixbench::Matrix<Integer, Storage> large_triangular{4};
    large_triangular(0, 0) = 2;
    large_triangular(0, 1) = 9;
    large_triangular(1, 1) = 3;
    large_triangular(1, 3) = -4;
    large_triangular(2, 2) = 4;
    large_triangular(3, 3) = 5;
    CHECK(matrixbench::determinant_bareiss(large_triangular) == 120);

    const Integer maximum = std::numeric_limits<Integer>::max();

    matrixbench::Matrix<Integer, Storage> result_overflow{2};
    result_overflow(0, 0) = maximum;
    result_overflow(1, 1) = 2;
    CHECK_THROWS_AS(matrixbench::determinant_bareiss(result_overflow),
                    std::overflow_error);

    matrixbench::Matrix<Integer, Storage> intermediate_overflow{2};
    intermediate_overflow(0, 0) = maximum;
    intermediate_overflow(0, 1) = maximum - 1;
    intermediate_overflow(1, 0) = maximum - 1;
    intermediate_overflow(1, 1) = maximum - 2;
    CHECK_THROWS_AS(matrixbench::determinant_bareiss(intermediate_overflow),
                    std::overflow_error);

    matrixbench::Matrix<Integer, Storage> sign_overflow{2};
    sign_overflow(0, 1) = 1;
    sign_overflow(1, 0) = std::numeric_limits<Integer>::min();
    CHECK_THROWS_AS(matrixbench::determinant_bareiss(sign_overflow),
                    std::overflow_error);
}

} // namespace

int main() {
    run_test("contiguous storage", [] {
        test_storage_basics<matrixbench::ContiguousStorage>();
    });
    run_test("jagged storage", [] {
        test_storage_basics<matrixbench::JaggedStorage>();
    });
    run_test("jagged partial construction", test_jagged_partial_construction_cleanup);
    run_test("contiguous copy exception", [] {
        test_copy_assignment_strong_guarantee<matrixbench::ContiguousStorage>();
    });
    run_test("jagged copy exception", [] {
        test_copy_assignment_strong_guarantee<matrixbench::JaggedStorage>();
    });
    run_test("contiguous matrix", [] {
        test_matrix_facade<matrixbench::ContiguousStorage>();
    });
    run_test("jagged matrix", [] {
        test_matrix_facade<matrixbench::JaggedStorage>();
    });
    run_test("contiguous gauss", [] {
        test_gauss<matrixbench::ContiguousStorage>();
    });
    run_test("jagged gauss", [] {
        test_gauss<matrixbench::JaggedStorage>();
    });
    run_test("contiguous bareiss", [] {
        test_bareiss<matrixbench::ContiguousStorage>();
    });
    run_test("jagged bareiss", [] {
        test_bareiss<matrixbench::JaggedStorage>();
    });

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }

    std::cout << "All MatrixBench tests passed\n";
    return 0;
}
