module;

#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

export module matrixbench.determinant;

export import matrixbench.matrix;

namespace matrixbench::detail {

template<std::signed_integral T>
T checked_multiply(T left, T right) {
    constexpr T minimum = std::numeric_limits<T>::min();
    constexpr T maximum = std::numeric_limits<T>::max();

    if (left == 0 || right == 0) {
        return 0;
    }

    bool overflow = false;
    if (left > 0) {
        if (right > 0) {
            overflow = left > maximum / right;
        } else {
            overflow = right < minimum / left;
        }
    } else if (right > 0) {
        overflow = left < minimum / right;
    } else {
        overflow = right < maximum / left;
    }

    if (overflow) {
        throw std::overflow_error{"integer multiplication overflow"};
    }
    return static_cast<T>(left * right);
}

template<std::signed_integral T>
T checked_subtract(T left, T right) {
    constexpr T minimum = std::numeric_limits<T>::min();
    constexpr T maximum = std::numeric_limits<T>::max();

    if ((right > 0 && left < minimum + right)
        || (right < 0 && left > maximum + right)) {
        throw std::overflow_error{"integer subtraction overflow"};
    }
    return static_cast<T>(left - right);
}

template<std::signed_integral T>
T checked_exact_divide(T numerator, T denominator) {
    if (denominator == 0) {
        throw std::logic_error{"Bareiss division by zero"};
    }
    if (numerator == std::numeric_limits<T>::min() && denominator == T{-1}) {
        throw std::overflow_error{"integer division overflow"};
    }
    if (numerator % denominator != 0) {
        throw std::logic_error{"Bareiss division is not exact"};
    }
    return static_cast<T>(numerator / denominator);
}

template<std::signed_integral T>
T checked_negate(T value) {
    if (value == std::numeric_limits<T>::min()) {
        throw std::overflow_error{"integer negation overflow"};
    }
    return static_cast<T>(-value);
}

} // namespace matrixbench::detail

export namespace matrixbench {

template<std::floating_point T, template<typename> typename Storage>
T determinant_gauss(
    const Matrix<T, Storage>& matrix,
    T epsilon = std::numeric_limits<T>::epsilon()
) {
    if (!std::isfinite(epsilon) || epsilon < T{}) {
        throw std::invalid_argument{"epsilon must be finite and nonnegative"};
    }

    const std::size_t size = matrix.size();
    if (size == 0) {
        return T{1};
    }

    Matrix<T, Storage> working{matrix};
    bool odd_swaps = false;

    for (std::size_t pivot_column = 0; pivot_column < size; ++pivot_column) {
        std::size_t pivot_row = pivot_column;
        T pivot_magnitude = std::abs(working(pivot_row, pivot_column));

        for (std::size_t row = pivot_column + 1; row < size; ++row) {
            const T magnitude = std::abs(working(row, pivot_column));
            if (magnitude > pivot_magnitude) {
                pivot_magnitude = magnitude;
                pivot_row = row;
            }
        }

        if (pivot_magnitude <= epsilon) {
            return T{};
        }

        if (pivot_row != pivot_column) {
            for (std::size_t column = 0; column < size; ++column) {
                std::swap(working(pivot_column, column), working(pivot_row, column));
            }
            odd_swaps = !odd_swaps;
        }

        const T pivot = working(pivot_column, pivot_column);
        for (std::size_t row = pivot_column + 1; row < size; ++row) {
            const T factor = working(row, pivot_column) / pivot;
            working(row, pivot_column) = T{};
            for (std::size_t column = pivot_column + 1; column < size; ++column) {
                working(row, column) -= factor * working(pivot_column, column);
            }
        }
    }

    T determinant = odd_swaps ? T{-1} : T{1};
    for (std::size_t diagonal = 0; diagonal < size; ++diagonal) {
        determinant *= working(diagonal, diagonal);
    }
    return determinant;
}

template<std::signed_integral T, template<typename> typename Storage>
T determinant_bareiss(const Matrix<T, Storage>& matrix) {
    const std::size_t size = matrix.size();
    if (size == 0) {
        return T{1};
    }
    if (size == 1) {
        return matrix(0, 0);
    }

    Matrix<T, Storage> working{matrix};
    T previous_pivot{1};
    bool odd_swaps = false;

    for (std::size_t pivot_column = 0; pivot_column + 1 < size; ++pivot_column) {
        std::size_t pivot_row = pivot_column;
        while (pivot_row < size && working(pivot_row, pivot_column) == T{}) {
            ++pivot_row;
        }

        if (pivot_row == size) {
            return T{};
        }

        if (pivot_row != pivot_column) {
            for (std::size_t column = 0; column < size; ++column) {
                std::swap(working(pivot_column, column), working(pivot_row, column));
            }
            odd_swaps = !odd_swaps;
        }

        const T pivot = working(pivot_column, pivot_column);
        for (std::size_t row = pivot_column + 1; row < size; ++row) {
            for (std::size_t column = pivot_column + 1; column < size; ++column) {
                const T left = detail::checked_multiply(pivot, working(row, column));
                const T right = detail::checked_multiply(
                    working(row, pivot_column),
                    working(pivot_column, column)
                );
                const T numerator = detail::checked_subtract(left, right);
                working(row, column) =
                    detail::checked_exact_divide(numerator, previous_pivot);
            }
            working(row, pivot_column) = T{};
        }
        previous_pivot = pivot;
    }

    T determinant = working(size - 1, size - 1);
    if (odd_swaps) {
        determinant = detail::checked_negate(determinant);
    }
    return determinant;
}

} // namespace matrixbench
