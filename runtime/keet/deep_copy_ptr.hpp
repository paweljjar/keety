#pragma once

#include <memory>
#include <utility>

namespace keet {

/// Transparent smart pointer with value-copy semantics.
/// Used by Pass 5 (Recursive Smart-Pointer Boxing) of the keety compiler to break
/// infinite-size cycles in self-referential or recursive types (e.g. class Node { let next: Node | null; })
/// while strictly preserving KeetScript's default deep value-copy semantics.
template <typename T>
class deep_copy_ptr {
public:
    using value_type = T;

    /// Default constructor: empty pointer (null)
    constexpr deep_copy_ptr() noexcept = default;

    /// Nullptr constructor
    constexpr deep_copy_ptr(std::nullptr_t) noexcept : ptr_(nullptr) {}

    /// Direct value constructor (allocates and stores a deep copy or moves value)
    explicit deep_copy_ptr(const T& val) : ptr_(std::make_unique<T>(val)) {}
    explicit deep_copy_ptr(T&& val) : ptr_(std::make_unique<T>(std::move(val))) {}

    /// Raw pointer adoption constructor
    explicit deep_copy_ptr(T* rawPtr) noexcept : ptr_(rawPtr) {}

    /// Deep copy constructor: clones the underlying value if present
    deep_copy_ptr(const deep_copy_ptr& other)
        : ptr_(other.ptr_ ? std::make_unique<T>(*other.ptr_) : nullptr) {}

    /// Move constructor: transfers ownership with zero allocations
    deep_copy_ptr(deep_copy_ptr&& other) noexcept = default;

    ~deep_copy_ptr() = default;

    /// Deep copy assignment: clones the underlying value
    deep_copy_ptr& operator=(const deep_copy_ptr& other) {
        if (this != &other) {
            ptr_ = other.ptr_ ? std::make_unique<T>(*other.ptr_) : nullptr;
        }
        return *this;
    }

    /// Move assignment: transfers ownership with zero allocations
    deep_copy_ptr& operator=(deep_copy_ptr&& other) noexcept = default;

    /// Assign from nullptr
    deep_copy_ptr& operator=(std::nullptr_t) noexcept {
        ptr_.reset();
        return *this;
    }

    /// Assign directly from value (creates or updates underlying value)
    deep_copy_ptr& operator=(const T& val) {
        ptr_ = std::make_unique<T>(val);
        return *this;
    }

    deep_copy_ptr& operator=(T&& val) {
        ptr_ = std::make_unique<T>(std::move(val));
        return *this;
    }

    // --- Dereference operators ---

    [[nodiscard]] T& operator*() noexcept { return *ptr_; }
    [[nodiscard]] const T& operator*() const noexcept { return *ptr_; }

    [[nodiscard]] T* operator->() noexcept { return ptr_.get(); }
    [[nodiscard]] const T* operator->() const noexcept { return ptr_.get(); }

    [[nodiscard]] T* get() noexcept { return ptr_.get(); }
    [[nodiscard]] const T* get() const noexcept { return ptr_.get(); }

    // --- State inspection ---

    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }
    [[nodiscard]] bool hasValue() const noexcept { return ptr_ != nullptr; }

    void reset() noexcept { ptr_.reset(); }

    // --- Comparison operators ---

    [[nodiscard]] bool operator==(const deep_copy_ptr& other) const {
        if (!ptr_ && !other.ptr_) return true;
        if (!ptr_ || !other.ptr_) return false;
        return *ptr_ == *other.ptr_;
    }

    [[nodiscard]] bool operator==(std::nullptr_t) const noexcept {
        return ptr_ == nullptr;
    }

    [[nodiscard]] bool operator==(const T& val) const {
        return ptr_ ? (*ptr_ == val) : false;
    }

private:
    std::unique_ptr<T> ptr_{nullptr};
};

/// Factory function
template <typename T, typename... Args>
[[nodiscard]] deep_copy_ptr<T> make_deep_copy(Args&&... args) {
    return deep_copy_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace keet
