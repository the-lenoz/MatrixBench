module;

#include <concepts>
#include <limits>

export module matrixbench.determinant;

export import matrixbench.matrix;

export namespace matrixbench {

template<std::floating_point T, template<typename> typename Storage>
T determinant_gauss(
    const Matrix<T, Storage>& matrix,
    T epsilon = std::numeric_limits<T>::epsilon()
);

template<std::signed_integral T, template<typename> typename Storage>
T determinant_bareiss(const Matrix<T, Storage>& matrix);

} // namespace matrixbench
