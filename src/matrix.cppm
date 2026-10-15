module;

#include <cstddef>

export module matrixbench.matrix;

export import matrixbench.storage;

export namespace matrixbench {

template<typename T, template<typename> typename StoragePolicy = ContiguousStorage>
    requires MatrixStorage<StoragePolicy<T>, T>
class Matrix final {
public:
    using value_type = T;
    using storage_type = StoragePolicy<T>;

    explicit Matrix(std::size_t size);
    ~Matrix();

    Matrix(const Matrix& other);
    Matrix& operator=(const Matrix& other);

    Matrix(Matrix&& other) noexcept;
    Matrix& operator=(Matrix&& other) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;

    T& operator()(std::size_t row, std::size_t column) noexcept;
    const T& operator()(std::size_t row, std::size_t column) const noexcept;

private:
    StoragePolicy<T> storage_;
};

} // namespace matrixbench
