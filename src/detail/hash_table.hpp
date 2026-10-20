// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace autolib::detail {

inline std::uint64_t mix(std::uint64_t x) noexcept {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    return x ^ (x >> 33);
}

/// Open addressing table numbering 64-bit keys from 0 in insertion order.
class key_numbering {
public:
    explicit key_numbering(std::size_t expected = 16) { rehash(std::bit_ceil(std::max<std::size_t>(16, 2 * expected))); }

    /// Returns the number of the key, adding it if absent; `added` tells which.
    std::int32_t insert(std::uint64_t key, bool& added) {
        if (2 * (keys_.size() + 1) > slots_.size())
            rehash(2 * slots_.size());
        std::size_t mask = slots_.size() - 1;
        for (std::size_t i = mix(key) & mask;; i = (i + 1) & mask) {
            std::int32_t id = slots_[i];
            if (id < 0) {
                id = static_cast<std::int32_t>(keys_.size());
                slots_[i] = id;
                keys_.push_back(key);
                added = true;
                return id;
            }
            if (keys_[static_cast<std::size_t>(id)] == key) {
                added = false;
                return id;
            }
        }
    }
    [[nodiscard]] std::uint64_t key(std::int32_t id) const noexcept { return keys_[static_cast<std::size_t>(id)]; }
    [[nodiscard]] std::size_t size() const noexcept { return keys_.size(); }

private:
    void rehash(std::size_t capacity) {
        slots_.assign(capacity, -1);
        std::size_t mask = capacity - 1;
        for (std::size_t id = 0; id < keys_.size(); ++id) {
            std::size_t i = mix(keys_[id]) & mask;
            while (slots_[i] >= 0)
                i = (i + 1) & mask;
            slots_[i] = static_cast<std::int32_t>(id);
        }
    }

    std::vector<std::int32_t> slots_;
    std::vector<std::uint64_t> keys_;
};

/// Open addressing table numbering sequences of integers from 0 in insertion order, stored contiguously.
class sequence_numbering {
public:
    sequence_numbering() { slots_.assign(64, -1); offsets_.push_back(0); }

    std::int32_t insert(std::span<const std::int32_t> seq, bool& added) {
        if (2 * (hashes_.size() + 1) > slots_.size())
            rehash(2 * slots_.size());
        std::uint64_t h = hash(seq);
        std::size_t mask = slots_.size() - 1;
        for (std::size_t i = h & mask;; i = (i + 1) & mask) {
            std::int32_t id = slots_[i];
            if (id < 0) {
                id = static_cast<std::int32_t>(hashes_.size());
                slots_[i] = id;
                hashes_.push_back(h);
                pool_.insert(pool_.end(), seq.begin(), seq.end());
                offsets_.push_back(pool_.size());
                added = true;
                return id;
            }
            if (hashes_[static_cast<std::size_t>(id)] == h && std::ranges::equal(at(id), seq)) {
                added = false;
                return id;
            }
        }
    }
    [[nodiscard]] std::span<const std::int32_t> at(std::int32_t id) const noexcept {
        auto i = static_cast<std::size_t>(id);
        return {pool_.data() + offsets_[i], offsets_[i + 1] - offsets_[i]};
    }
    [[nodiscard]] std::size_t size() const noexcept { return hashes_.size(); }

private:
    static std::uint64_t hash(std::span<const std::int32_t> seq) noexcept {
        std::uint64_t h = seq.size();
        for (std::int32_t v : seq)
            h = mix(h ^ static_cast<std::uint32_t>(v)) + 0x9e3779b97f4a7c15ULL;
        return h;
    }
    void rehash(std::size_t capacity) {
        slots_.assign(capacity, -1);
        std::size_t mask = capacity - 1;
        for (std::size_t id = 0; id < hashes_.size(); ++id) {
            std::size_t i = hashes_[id] & mask;
            while (slots_[i] >= 0)
                i = (i + 1) & mask;
            slots_[i] = static_cast<std::int32_t>(id);
        }
    }

    std::vector<std::int32_t> slots_;
    std::vector<std::uint64_t> hashes_;
    std::vector<std::int32_t> pool_;
    std::vector<std::size_t> offsets_;
};

/// Key of a pair of states, possibly no_state (-1).
inline std::uint64_t pair_key(std::int32_t p, std::int32_t q) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(p + 1)) << 32) | static_cast<std::uint32_t>(q + 1);
}
inline std::int32_t pair_first(std::uint64_t key) noexcept { return static_cast<std::int32_t>(key >> 32) - 1; }
inline std::int32_t pair_second(std::uint64_t key) noexcept { return static_cast<std::int32_t>(key & 0xffffffffU) - 1; }

}  // namespace autolib::detail
