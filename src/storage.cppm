module;

#include <concepts>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

export module matrixbench.storage;

export namespace matrixbench {

template<typename T>
class ContiguousStorage final {
public:
    using value_type = T;

    explicit ContiguousStorage(std::size_t size) {
        validate_size(size);
        size_ = size;
        if (size_ != 0) {
            data_ = new T[size_ * size_]{};
        }
    }

    ~ContiguousStorage() {
        delete[] data_;
    }

    ContiguousStorage(const ContiguousStorage& other)
        : ContiguousStorage(other.size_) {
        try {
            const std::size_t element_count = size_ * size_;
            for (std::size_t index = 0; index < element_count; ++index) {
                data_[index] = other.data_[index];
            }
        } catch (...) {
            delete[] data_;
            data_ = nullptr;
            size_ = 0;
            throw;
        }
    }

    ContiguousStorage& operator=(const ContiguousStorage& other) {
        if (this != &other) {
            ContiguousStorage copy{other};
            swap(copy);
        }
        return *this;
    }

    ContiguousStorage(ContiguousStorage&& other) noexcept
        : size_(std::exchange(other.size_, 0)),
          data_(std::exchange(other.data_, nullptr)) {}

    ContiguousStorage& operator=(ContiguousStorage&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            size_ = std::exchange(other.size_, 0);
            data_ = std::exchange(other.data_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    T& operator()(std::size_t row, std::size_t column) noexcept {
        return data_[row * size_ + column];
    }

    const T& operator()(std::size_t row, std::size_t column) const noexcept {
        return data_[row * size_ + column];
    }

private:
    static void validate_size(std::size_t size) {
        if (size != 0 && size > std::numeric_limits<std::size_t>::max() / size) {
            throw std::length_error{"matrix element count overflows size_t"};
        }
    }

    void swap(ContiguousStorage& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(data_, other.data_);
    }

    std::size_t size_{};
    T* data_{};
};

template<typename T>
class JaggedStorage final {
public:
    using value_type = T;

    explicit JaggedStorage(std::size_t size) {
        validate_size(size);
        if (size == 0) {
            return;
        }

        size_ = size;
        rows_ = new T*[size_]{};

        std::size_t constructed_rows = 0;
        try {
            for (; constructed_rows < size_; ++constructed_rows) {
                rows_[constructed_rows] = new T[size_]{};
            }
        } catch (...) {
            for (std::size_t row = 0; row < constructed_rows; ++row) {
                delete[] rows_[row];
            }
            delete[] rows_;
            rows_ = nullptr;
            size_ = 0;
            throw;
        }
    }

    ~JaggedStorage() {
        release();
    }

    JaggedStorage(const JaggedStorage& other)
        : JaggedStorage(other.size_) {
        try {
            for (std::size_t row = 0; row < size_; ++row) {
                for (std::size_t column = 0; column < size_; ++column) {
                    rows_[row][column] = other.rows_[row][column];
                }
            }
        } catch (...) {
            release();
            throw;
        }
    }

    JaggedStorage& operator=(const JaggedStorage& other) {
        if (this != &other) {
            JaggedStorage copy{other};
            swap(copy);
        }
        return *this;
    }

    JaggedStorage(JaggedStorage&& other) noexcept
        : size_(std::exchange(other.size_, 0)),
          rows_(std::exchange(other.rows_, nullptr)) {}

    JaggedStorage& operator=(JaggedStorage&& other) noexcept {
        if (this != &other) {
            release();
            size_ = std::exchange(other.size_, 0);
            rows_ = std::exchange(other.rows_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    T& operator()(std::size_t row, std::size_t column) noexcept {
        return rows_[row][column];
    }

    const T& operator()(std::size_t row, std::size_t column) const noexcept {
        return rows_[row][column];
    }

private:
    static void validate_size(std::size_t size) {
        if (size != 0 && size > std::numeric_limits<std::size_t>::max() / size) {
            throw std::length_error{"matrix element count overflows size_t"};
        }
    }

    void release() noexcept {
        if (rows_ != nullptr) {
            for (std::size_t row = 0; row < size_; ++row) {
                delete[] rows_[row];
            }
            delete[] rows_;
        }
        rows_ = nullptr;
        size_ = 0;
    }

    void swap(JaggedStorage& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(rows_, other.rows_);
    }

    std::size_t size_{};
    T** rows_{};
};

template<typename Storage, typename T>
concept MatrixStorage =
    std::same_as<typename Storage::value_type, T>
    && std::copy_constructible<Storage>
    && std::movable<Storage>
    && requires(Storage storage,
                const Storage const_storage,
                std::size_t row,
                std::size_t column) {
        { storage.size() } noexcept -> std::same_as<std::size_t>;
        { storage(row, column) } noexcept -> std::same_as<T&>;
        { const_storage(row, column) } noexcept -> std::same_as<const T&>;
    };

} // namespace matrixbench
