#ifndef SIRPG_OBJECT_POOL_HPP
#define SIRPG_OBJECT_POOL_HPP

#include <cstddef>
#include <cstdint>
#include <array>
#include <utility>
#include <expected>
#include <span>
#include <functional>

namespace sirpg::core {

enum class PoolError {
    PoolExhausted,
    InvalidPointer
};

template<typename T, std::size_t Capacity>
class ObjectPool {
public:
    ObjectPool() noexcept {
        for (std::size_t i = 0; i < Capacity; ++i) {
            m_freeIndices[i] = i;
            m_active[i] = false;
        }
        m_freeCount = Capacity;
    }

    ~ObjectPool() = default;

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    template<typename... Args>
    std::expected<T*, PoolError> spawn(Args&&... args) noexcept {
        if (m_freeCount == 0) {
            return std::unexpected(PoolError::PoolExhausted);
        }

        std::size_t index = m_freeIndices[--m_freeCount];
        m_active[index] = true;

        T* elementPtr = reinterpret_cast<T*>(&m_storage[index]);
        ::new (static_cast<void*>(elementPtr)) T(std::forward<Args>(args)...);

        return elementPtr;
    }

    std::expected<void, PoolError> recycle(T* item) noexcept {
        if (item < reinterpret_cast<T*>(&m_storage[0]) ||
            item >= reinterpret_cast<T*>(&m_storage[Capacity])) {
            return std::unexpected(PoolError::InvalidPointer);
        }

        std::size_t index = static_cast<std::size_t>(item - reinterpret_cast<T*>(&m_storage[0]));
        if (!m_active[index]) {
            return {}; // Already recycled
        }

        item->~T();
        m_active[index] = false;
        m_freeIndices[m_freeCount++] = index;

        return {};
    }

    void clear() noexcept {
        for (std::size_t i = 0; i < Capacity; ++i) {
            if (m_active[i]) {
                reinterpret_cast<T*>(&m_storage[i])->~T();
                m_active[i] = false;
            }
            m_freeIndices[i] = i;
        }
        m_freeCount = Capacity;
    }

    template<typename Func>
    void forEachActive(Func&& func) noexcept {
        for (std::size_t i = 0; i < Capacity; ++i) {
            if (m_active[i]) {
                func(*reinterpret_cast<T*>(&m_storage[i]));
            }
        }
    }

    [[nodiscard]] std::size_t getCapacity() const noexcept { return Capacity; }
    [[nodiscard]] std::size_t getActiveCount() const noexcept { return Capacity - m_freeCount; }
    [[nodiscard]] std::size_t getFreeCount() const noexcept { return m_freeCount; }
    [[nodiscard]] bool isActive(std::size_t index) const noexcept { return index < Capacity && m_active[index]; }

private:
    alignas(alignof(T)) std::array<std::byte[sizeof(T)], Capacity> m_storage;
    std::array<std::size_t, Capacity> m_freeIndices{};
    std::array<bool, Capacity> m_active{};
    std::size_t m_freeCount{Capacity};
};

} // namespace sirpg::core

#endif // SIRPG_OBJECT_POOL_HPP
