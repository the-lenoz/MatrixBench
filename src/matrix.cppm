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

    explicit Matrix(std::size_t size) : storage_(size) {}
    ~Matrix() = default;

    Matrix(const Matrix& other) = default;
    Matrix& operator=(const Matrix& other) = default;

    Matrix(Matrix&& other) noexcept = default;
    Matrix& operator=(Matrix&& other) noexcept = default;

    [[nodiscard]] std::size_t size() const noexcept {
        return storage_.size();
    }

    T& operator()(std::size_t row, std::size_t column) noexcept {
        return storage_(row, column);
    }

    const T& operator()(std::size_t row, std::size_t column) const noexcept {
        return storage_(row, column);
    }

private:
    StoragePolicy<T> storage_;
};

} // namespace matrixbench
