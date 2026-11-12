#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <concepts>
#include <cstddef>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

import matrixbench.determinant;

namespace {

using testing::DoubleNear;
using testing::Each;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;

// A non-owning row-major range; works with either layout without STL containers.
template<typename Square>
class Elements final {
public:
    using value_type = typename Square::value_type;
    struct const_iterator {
        using value_type = typename Square::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        using iterator_category = std::forward_iterator_tag;
        const Square* square;
        std::size_t index;
        reference operator*() const {
            return (*square)(index / square->size(), index % square->size());
        }
        pointer operator->() const { return &**this; }
        const_iterator& operator++() { ++index; return *this; }
        const_iterator operator++(int) { auto previous = *this; ++*this; return previous; }
        bool operator==(const const_iterator&) const = default;
    };
    explicit Elements(const Square& square) : square_(square) {}
    std::size_t size() const { return square_.size() * square_.size(); }
    bool empty() const { return size() == 0; }
    const_iterator begin() const { return {&square_, 0}; }
    const_iterator end() const { return {&square_, square_.size() * square_.size()}; }
private:
    const Square& square_;
};

template<typename Square, typename T, std::size_t N>
Square make_square(const T (&values)[N][N]) {
    Square square{N};
    for (std::size_t row = 0; row < N; ++row) {
        for (std::size_t column = 0; column < N; ++column) {
            square(row, column) = values[row][column];
        }
    }
    return square;
}

template<typename T>
void self_copy_assign(T& value) {
    const T* alias = &value;
    value = *alias;
}

template<typename T>
void self_move_assign(T& value) {
    T* alias = &value;
    value = std::move(*alias);
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

struct TrackingScope final {
    TrackingScope() { TrackedValue::reset(); }
    ~TrackingScope() { TrackedValue::reset(); }
};

static_assert(matrixbench::MatrixStorage<matrixbench::ContiguousStorage<int>, int>);
static_assert(matrixbench::MatrixStorage<matrixbench::JaggedStorage<int>, int>);
static_assert(std::same_as<matrixbench::Matrix<int>::value_type, int>);
static_assert(std::same_as<matrixbench::Matrix<int>::storage_type,
                           matrixbench::ContiguousStorage<int>>);

template<typename T>
class Ownership : public testing::Test {};
using Owners = testing::Types<matrixbench::ContiguousStorage<int>,
    matrixbench::JaggedStorage<int>,
    matrixbench::Matrix<int, matrixbench::ContiguousStorage>,
    matrixbench::Matrix<int, matrixbench::JaggedStorage>>;
TYPED_TEST_SUITE(Ownership, Owners);

TYPED_TEST(Ownership, Empty) {
    // Arrange
    const std::size_t size = 0;

    // Act
    const TypeParam square{size};

    // Assert
    EXPECT_THAT(square.size(), Eq(0U));
    EXPECT_THAT(Elements(square), IsEmpty());
}

TYPED_TEST(Ownership, SingleValueInitialized) {
    // Arrange
    const std::size_t size = 1;

    // Act
    const TypeParam square{size};

    // Assert
    EXPECT_THAT(square.size(), Eq(1U));
    EXPECT_THAT(Elements(square), ElementsAre(0));
}

TYPED_TEST(Ownership, ValuesInitialized) {
    // Arrange
    const std::size_t size = 3;

    // Act
    const TypeParam square{size};

    // Assert
    EXPECT_THAT(square.size(), Eq(3U));
    EXPECT_THAT(Elements(square), Each(Eq(0)));
}

TYPED_TEST(Ownership, CopyConstruction) {
    // Arrange
    const int values[2][2]{{1, 2}, {3, 4}};
    auto source = make_square<TypeParam>(values);

    // Act
    const TypeParam result{source};
    source(0, 0) = 99;

    // Assert
    EXPECT_THAT(result.size(), Eq(2U));
    EXPECT_THAT(Elements(result), ElementsAre(1, 2, 3, 4));
}

TYPED_TEST(Ownership, CopyAssignment) {
    // Arrange
    const int values[2][2]{{1, 2}, {3, 4}};
    auto source = make_square<TypeParam>(values);
    TypeParam result{1};

    // Act
    result = source;
    source(0, 0) = 99;

    // Assert
    EXPECT_THAT(result.size(), Eq(2U));
    EXPECT_THAT(Elements(result), ElementsAre(1, 2, 3, 4));
}

TYPED_TEST(Ownership, MoveConstruction) {
    // Arrange
    const int values[2][2]{{1, 2}, {3, 4}};
    auto source = make_square<TypeParam>(values);

    // Act
    const TypeParam result{std::move(source)};

    // Assert
    EXPECT_THAT(result.size(), Eq(2U));
    EXPECT_THAT(Elements(result), ElementsAre(1, 2, 3, 4));
    EXPECT_THAT(source.size(), Eq(0U));
    EXPECT_THAT(Elements(source), IsEmpty());
}

TYPED_TEST(Ownership, MoveAssignment) {
    // Arrange
    const int values[2][2]{{1, 2}, {3, 4}};
    auto source = make_square<TypeParam>(values);
    TypeParam result{1};

    // Act
    result = std::move(source);

    // Assert
    EXPECT_THAT(result.size(), Eq(2U));
    EXPECT_THAT(Elements(result), ElementsAre(1, 2, 3, 4));
    EXPECT_THAT(source.size(), Eq(0U));
    EXPECT_THAT(Elements(source), IsEmpty());
}

TYPED_TEST(Ownership, SelfCopyAssignment) {
    // Arrange
    const int values[2][2]{{1, 2}, {3, 4}};
    auto source = make_square<TypeParam>(values);

    // Act
    self_copy_assign(source);

    // Assert
    EXPECT_THAT(source.size(), Eq(2U));
    EXPECT_THAT(Elements(source), ElementsAre(1, 2, 3, 4));
}

TYPED_TEST(Ownership, SelfMoveAssignment) {
    // Arrange
    const int values[2][2]{{1, 2}, {3, 4}};
    auto source = make_square<TypeParam>(values);

    // Act
    self_move_assign(source);

    // Assert
    EXPECT_THAT(source.size(), Eq(2U));
    EXPECT_THAT(Elements(source), ElementsAre(1, 2, 3, 4));
}

TYPED_TEST(Ownership, ConstAccess) {
    // Arrange
    const int values[2][2]{{1, 2}, {3, 4}};
    auto source = make_square<TypeParam>(values);

    // Act
    const TypeParam& view = source;

    // Assert
    EXPECT_THAT(view(0, 1), Eq(2));
    EXPECT_THAT(view(1, 0), Eq(3));
}

TYPED_TEST(Ownership, RejectsSizeOverflow) {
    // Arrange
    const auto size = std::numeric_limits<std::size_t>::max();

    // Act / Assert
    EXPECT_THROW((TypeParam{size}), std::length_error);
}

template<typename T>
class ExceptionSafety : public testing::Test {};
using TrackedOwners = testing::Types<matrixbench::ContiguousStorage<TrackedValue>,
    matrixbench::JaggedStorage<TrackedValue>,
    matrixbench::Matrix<TrackedValue, matrixbench::ContiguousStorage>,
    matrixbench::Matrix<TrackedValue, matrixbench::JaggedStorage>>;
TYPED_TEST_SUITE(ExceptionSafety, TrackedOwners);

TYPED_TEST(ExceptionSafety, CopyAssignmentPreservesDestinationOnFailure) {
    // Arrange
    TrackingScope tracking;
    TypeParam source{2};
    source(0, 0).value = 1;
    TypeParam destination{1};
    destination(0, 0).value = 99;
    const auto alive_before = TrackedValue::alive;
    TrackedValue::assignment_budget = 1;

    // Act / Assert
    EXPECT_THROW(destination = source, TestFailure);
    EXPECT_THAT(destination.size(), Eq(1U));
    EXPECT_THAT(destination(0, 0).value, Eq(99));
    EXPECT_THAT(source(0, 0).value, Eq(1));
    EXPECT_THAT(TrackedValue::alive, Eq(alive_before));
}

TEST(ExceptionSafety, JaggedPartialConstructionReleasesElements) {
    // Arrange
    TrackingScope tracking;
    const auto alive_before = TrackedValue::alive;
    TrackedValue::default_budget = 5;

    // Act / Assert
    EXPECT_THROW((matrixbench::JaggedStorage<TrackedValue>{3}), TestFailure);
    EXPECT_THAT(TrackedValue::alive, Eq(alive_before));
}

struct Contiguous {
    template<typename T> using Matrix = matrixbench::Matrix<T, matrixbench::ContiguousStorage>;
};
struct Jagged {
    template<typename T> using Matrix = matrixbench::Matrix<T, matrixbench::JaggedStorage>;
};
template<typename T>
class Determinant : public testing::Test {};
using Layouts = testing::Types<Contiguous, Jagged>;
TYPED_TEST_SUITE(Determinant, Layouts);

TYPED_TEST(Determinant, GaussEmptyDeterminantIsOne) {
    // Arrange
    typename TypeParam::template Matrix<double> empty{0};

    // Act
    const auto result = matrixbench::determinant_gauss(empty);

    // Assert
    EXPECT_THAT(result, DoubleNear(1, 1.0e-10));
}

TYPED_TEST(Determinant, GaussSingleElement) {
    // Arrange
    typename TypeParam::template Matrix<double> single{1};
    single(0, 0) = -3.5;

    // Act
    const auto result = matrixbench::determinant_gauss(single);

    // Assert
    EXPECT_THAT(result, DoubleNear(-3.5, 1.0e-10));
}

TYPED_TEST(Determinant, GaussKnownResultPreservesInput) {
    // Arrange
    typename TypeParam::template Matrix<double> matrix{2};
    matrix(0, 0) = 1.0;
    matrix(0, 1) = 2.0;
    matrix(1, 0) = 3.0;
    matrix(1, 1) = 4.0;

    // Act
    const auto result = matrixbench::determinant_gauss(matrix);

    // Assert
    EXPECT_THAT(result, DoubleNear(-2, 1.0e-10));
    EXPECT_THAT(Elements(matrix), ElementsAre(1, 2, 3, 4));
}

TYPED_TEST(Determinant, GaussRowSwapChangesSign) {
    // Arrange
    typename TypeParam::template Matrix<double> row_swap{2};
    row_swap(0, 0) = 0.0;
    row_swap(0, 1) = 2.0;
    row_swap(1, 0) = 3.0;
    row_swap(1, 1) = 4.0;

    // Act
    const auto result = matrixbench::determinant_gauss(row_swap);

    // Assert
    EXPECT_THAT(result, DoubleNear(-6, 1.0e-10));
}

TYPED_TEST(Determinant, GaussTriangular) {
    // Arrange
    typename TypeParam::template Matrix<double> triangular{3};
    triangular(0, 0) = 2.0;
    triangular(0, 1) = 8.0;
    triangular(0, 2) = -1.0;
    triangular(1, 1) = 3.0;
    triangular(1, 2) = 5.0;
    triangular(2, 2) = 4.0;

    // Act
    const auto result = matrixbench::determinant_gauss(triangular);

    // Assert
    EXPECT_THAT(result, DoubleNear(24, 1.0e-10));
}

TYPED_TEST(Determinant, GaussSingularIsZero) {
    // Arrange
    typename TypeParam::template Matrix<double> singular{2};
    singular(0, 0) = 1.0;
    singular(0, 1) = 2.0;
    singular(1, 0) = 2.0;
    singular(1, 1) = 4.0;

    // Act
    const auto result = matrixbench::determinant_gauss(singular);

    // Assert
    EXPECT_THAT(result, Eq(0.0));
}

TYPED_TEST(Determinant, GaussBelowTolerance) {
    // Arrange
    typename TypeParam::template Matrix<double> near_zero{2};
    near_zero(0, 0) = 1.0e-8;
    near_zero(1, 1) = 1.0;

    // Act
    const auto result = matrixbench::determinant_gauss(near_zero, 1.0e-7);

    // Assert
    EXPECT_THAT(result, Eq(0.0));
}

TYPED_TEST(Determinant, GaussAboveTolerance) {
    // Arrange
    typename TypeParam::template Matrix<double> near_zero{2};
    near_zero(0, 0) = 1.0e-8;
    near_zero(1, 1) = 1.0;

    // Act
    const auto result = matrixbench::determinant_gauss(near_zero, 1.0e-10);

    // Assert
    EXPECT_THAT(result, DoubleNear(1.0e-8, 1.0e-15));
}

TYPED_TEST(Determinant, BareissEmptyDeterminantIsOne) {
    // Arrange
    typename TypeParam::template Matrix<long long> empty{0};

    // Act
    const auto result = matrixbench::determinant_bareiss(empty);

    // Assert
    EXPECT_THAT(result, Eq(1));
}

TYPED_TEST(Determinant, BareissSingleElement) {
    // Arrange
    typename TypeParam::template Matrix<long long> single{1};
    single(0, 0) = -7;

    // Act
    const auto result = matrixbench::determinant_bareiss(single);

    // Assert
    EXPECT_THAT(result, Eq(-7));
}

TYPED_TEST(Determinant, BareissKnownResultPreservesInput) {
    // Arrange
    typename TypeParam::template Matrix<long long> matrix{2};
    matrix(0, 0) = 1;
    matrix(0, 1) = 2;
    matrix(1, 0) = 3;
    matrix(1, 1) = 4;

    // Act
    const auto result = matrixbench::determinant_bareiss(matrix);

    // Assert
    EXPECT_THAT(result, Eq(-2));
    EXPECT_THAT(Elements(matrix), ElementsAre(1, 2, 3, 4));
}

TYPED_TEST(Determinant, BareissRowSwapChangesSign) {
    // Arrange
    typename TypeParam::template Matrix<long long> row_swap{2};
    row_swap(0, 0) = 0;
    row_swap(0, 1) = 2;
    row_swap(1, 0) = 3;
    row_swap(1, 1) = 4;

    // Act
    const auto result = matrixbench::determinant_bareiss(row_swap);

    // Assert
    EXPECT_THAT(result, Eq(-6));
}

TYPED_TEST(Determinant, BareissDiagonal) {
    // Arrange
    typename TypeParam::template Matrix<long long> diagonal{3};
    diagonal(0, 0) = 2;
    diagonal(1, 1) = -3;
    diagonal(2, 2) = 4;

    // Act
    const auto result = matrixbench::determinant_bareiss(diagonal);

    // Assert
    EXPECT_THAT(result, Eq(-24));
}

TYPED_TEST(Determinant, BareissSingularIsZero) {
    // Arrange
    typename TypeParam::template Matrix<long long> singular{2};
    singular(0, 0) = 1;
    singular(0, 1) = 2;
    singular(1, 0) = 2;
    singular(1, 1) = 4;

    // Act
    const auto result = matrixbench::determinant_bareiss(singular);

    // Assert
    EXPECT_THAT(result, Eq(0));
}

TYPED_TEST(Determinant, BareissExactDivision) {
    // Arrange
    typename TypeParam::template Matrix<long long> known{3};
    known(0, 0) = 2;
    known(0, 1) = 5;
    known(0, 2) = 3;
    known(1, 0) = 1;
    known(1, 1) = -2;
    known(1, 2) = -1;
    known(2, 0) = 1;
    known(2, 1) = 3;
    known(2, 2) = 4;

    // Act
    const auto result = matrixbench::determinant_bareiss(known);

    // Assert
    EXPECT_THAT(result, Eq(-20));
}

TYPED_TEST(Determinant, BareissTriangular) {
    // Arrange
    typename TypeParam::template Matrix<long long> large_triangular{4};
    large_triangular(0, 0) = 2;
    large_triangular(0, 1) = 9;
    large_triangular(1, 1) = 3;
    large_triangular(1, 3) = -4;
    large_triangular(2, 2) = 4;
    large_triangular(3, 3) = 5;

    // Act
    const auto result = matrixbench::determinant_bareiss(large_triangular);

    // Assert
    EXPECT_THAT(result, Eq(120));
}

TYPED_TEST(Determinant, BareissRejectsResultOverflow) {
    // Arrange
    typename TypeParam::template Matrix<long long> result_overflow{2};
    result_overflow(0, 0) = std::numeric_limits<long long>::max();
    result_overflow(1, 1) = 2;

    // Act / Assert
    EXPECT_THROW(matrixbench::determinant_bareiss(result_overflow), std::overflow_error);
}

TYPED_TEST(Determinant, BareissRejectsIntermediateOverflow) {
    // Arrange
    typename TypeParam::template Matrix<long long> intermediate_overflow{2};
    intermediate_overflow(0, 0) = std::numeric_limits<long long>::max();
    intermediate_overflow(0, 1) = std::numeric_limits<long long>::max() - 1;
    intermediate_overflow(1, 0) = std::numeric_limits<long long>::max() - 1;
    intermediate_overflow(1, 1) = std::numeric_limits<long long>::max() - 2;

    // Act / Assert
    EXPECT_THROW(matrixbench::determinant_bareiss(intermediate_overflow), std::overflow_error);
}

TYPED_TEST(Determinant, BareissRejectsSignOverflow) {
    // Arrange
    typename TypeParam::template Matrix<long long> sign_overflow{2};
    sign_overflow(0, 1) = 1;
    sign_overflow(1, 0) = std::numeric_limits<long long>::min();

    // Act / Assert
    EXPECT_THROW(matrixbench::determinant_bareiss(sign_overflow), std::overflow_error);
}

TYPED_TEST(Determinant, GaussRejectsNegativeTolerance) {
    // Arrange
    typename TypeParam::template Matrix<double> matrix{1};
    const double epsilon = -1.0;

    // Act / Assert
    EXPECT_THROW(matrixbench::determinant_gauss(matrix, epsilon), std::invalid_argument);
}

TYPED_TEST(Determinant, GaussRejectsInfiniteTolerance) {
    // Arrange
    typename TypeParam::template Matrix<double> matrix{1};
    const double epsilon = std::numeric_limits<double>::infinity();

    // Act / Assert
    EXPECT_THROW(matrixbench::determinant_gauss(matrix, epsilon), std::invalid_argument);
}

TYPED_TEST(Determinant, GaussRejectsNanTolerance) {
    // Arrange
    typename TypeParam::template Matrix<double> matrix{1};
    const double epsilon = std::numeric_limits<double>::quiet_NaN();

    // Act / Assert
    EXPECT_THROW(matrixbench::determinant_gauss(matrix, epsilon), std::invalid_argument);
}

} // namespace
