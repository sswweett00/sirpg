#ifndef SIRPG_ARENA_ALLOCATOR_HPP
#define SIRPG_ARENA_ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <concepts>
#include <expected>
#include <span>

namespace sirpg::core {

enum class AllocatorError {
    OutOfMemory,
    InvalidAlignment
};

class ArenaAllocator {
public:
    explicit ArenaAllocator(std::size_t capacityBytes)
        : m_capacity(capacityBytes), m_offset(0) {
        m_buffer = static_cast<std::byte*>(::operator new(m_capacity, std::align_val_t{64}));
    }

    ~ArenaAllocator() {
        if (m_buffer) {
            ::operator delete(m_buffer, std::align_val_t{64});
            m_buffer = nullptr;
        }
    }

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&&) noexcept = delete;
    ArenaAllocator& operator=(ArenaAllocator&&) noexcept = delete;

    std::expected<void*, AllocatorError> allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) noexcept {
        std::size_t currentPtr = reinterpret_cast<std::size_t>(m_buffer + m_offset);
        std::size_t padding = (alignment - (currentPtr % alignment)) % alignment;

        if (m_offset + padding + size > m_capacity) {
            return std::unexpected(AllocatorError::OutOfMemory);
        }

        m_offset += padding;
        void* ptr = m_buffer + m_offset;
        m_offset += size;
        return ptr;
    }

    template<typename T, typename... Args>
    std::expected<T*, AllocatorError> create(Args&&... args) noexcept {
        auto rawResult = allocate(sizeof(T), alignof(T));
        if (!rawResult) {
            return std::unexpected(rawResult.error());
        }
        T* ptr = static_cast<T*>(rawResult.value());
        return ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
    }

    void reset() noexcept {
        m_offset = 0;
    }

    [[nodiscard]] std::size_t getCapacity() const noexcept { return m_capacity; }
    [[nodiscard]] std::size_t getUsed() const noexcept { return m_offset; }
    [[nodiscard]] std::size_t getRemaining() const noexcept { return m_capacity - m_offset; }

private:
    std::byte* m_buffer{nullptr};
    std::size_t m_capacity{0};
    std::size_t m_offset{0};
};

} // namespace sirpg::core

#endif // SIRPG_ARENA_ALLOCATOR_HPP
