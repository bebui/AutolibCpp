// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace autolib::detail {

/// A vector storing up to N elements inline, without heap allocation. Elements must be nothrow movable.
template <class T, std::size_t N>
class small_vector {
    static_assert(std::is_nothrow_move_constructible_v<T>);

public:
    using value_type = T;
    using iterator = T*;
    using const_iterator = const T*;

    small_vector() noexcept = default;
    small_vector(std::initializer_list<T> values) {
        reserve(values.size());
        for (const T& v : values)
            push_back(v);
    }
    small_vector(const small_vector& other) {
        reserve(other.size_);
        std::uninitialized_copy_n(other.data(), other.size_, data());
        size_ = other.size_;
    }
    small_vector(small_vector&& other) noexcept { take(std::move(other)); }
    small_vector& operator=(const small_vector& other) {
        if (this != &other) {
            small_vector copy(other);
            *this = std::move(copy);
        }
        return *this;
    }
    small_vector& operator=(small_vector&& other) noexcept {
        if (this != &other) {
            release();
            take(std::move(other));
        }
        return *this;
    }
    small_vector& operator=(std::initializer_list<T> values) { return *this = small_vector(values); }
    ~small_vector() { release(); }

    [[nodiscard]] T* data() noexcept { return heap_ ? heap_ : std::launder(reinterpret_cast<T*>(inline_)); }
    [[nodiscard]] const T* data() const noexcept { return heap_ ? heap_ : std::launder(reinterpret_cast<const T*>(inline_)); }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] T* begin() noexcept { return data(); }
    [[nodiscard]] T* end() noexcept { return data() + size_; }
    [[nodiscard]] const T* begin() const noexcept { return data(); }
    [[nodiscard]] const T* end() const noexcept { return data() + size_; }
    [[nodiscard]] T& operator[](std::size_t i) noexcept { return data()[i]; }
    [[nodiscard]] const T& operator[](std::size_t i) const noexcept { return data()[i]; }
    [[nodiscard]] T& back() noexcept { return data()[size_ - 1]; }

    void reserve(std::size_t capacity) {
        if (capacity <= capacity_)
            return;
        auto grown = std::max<std::size_t>(capacity, 2 * capacity_);
        T* fresh = std::allocator<T>{}.allocate(grown);
        std::uninitialized_move_n(data(), size_, fresh);
        std::destroy_n(data(), size_);
        if (heap_)
            std::allocator<T>{}.deallocate(heap_, capacity_);
        heap_ = fresh;
        capacity_ = static_cast<std::uint32_t>(grown);
    }

    template <class... Args>
    T& emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            // The argument may refer to an element : it is built before the elements move
            T value(std::forward<Args>(args)...);
            reserve(size_ + 1);
            return *std::construct_at(data() + size_++, std::move(value));
        }
        return *std::construct_at(data() + size_++, std::forward<Args>(args)...);
    }
    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(std::move(value)); }
    void pop_back() noexcept { std::destroy_at(data() + --size_); }
    void clear() noexcept {
        std::destroy_n(data(), size_);
        size_ = 0;
    }

private:
    void release() noexcept {
        std::destroy_n(data(), size_);
        if (heap_)
            std::allocator<T>{}.deallocate(heap_, capacity_);
        heap_ = nullptr;
        size_ = 0;
        capacity_ = N;
    }
    /// Takes the elements of other, which must hold no element of this vector
    void take(small_vector&& other) noexcept {
        if (other.heap_) {
            heap_ = std::exchange(other.heap_, nullptr);
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, static_cast<std::uint32_t>(N));
        } else {
            std::uninitialized_move_n(other.data(), other.size_, data());
            size_ = other.size_;
            other.clear();
        }
    }

    T* heap_ = nullptr;
    std::uint32_t size_ = 0;
    std::uint32_t capacity_ = N;
    alignas(T) std::byte inline_[N * sizeof(T)];
};

}  // namespace autolib::detail
