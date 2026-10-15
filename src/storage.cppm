module;

#include <concepts>
#include <cstddef>

export module matrixbench.storage;

export namespace matrixbench {

template<typename T>
class ContiguousStorage final {
public:
    using value_type = T;

    explicit ContiguousStorage(std::size_t size);
    ~ContiguousStorage();

    ContiguousStorage(const ContiguousStorage& other);
    ContiguousStorage& operator=(const ContiguousStorage& other);

    ContiguousStorage(ContiguousStorage&& other) noexcept;
    ContiguousStorage& operator=(ContiguousStorage&& other) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;

    T& operator()(std::size_t row, std::size_t column) noexcept;
    const T& operator()(std::size_t row, std::size_t column) const noexcept;

private:
    std::size_t size_{};
    T* data_{};
};

template<typename T>
class JaggedStorage final {
public:
    using value_type = T;

    explicit JaggedStorage(std::size_t size);
    ~JaggedStorage();

    JaggedStorage(const JaggedStorage& other);
    JaggedStorage& operator=(const JaggedStorage& other);

    JaggedStorage(JaggedStorage&& other) noexcept;
    JaggedStorage& operator=(JaggedStorage&& other) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;

    T& operator()(std::size_t row, std::size_t column) noexcept;
    const T& operator()(std::size_t row, std::size_t column) const noexcept;

private:
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
