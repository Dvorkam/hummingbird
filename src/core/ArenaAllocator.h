#pragma once

#include <cstddef>
#include <vector>
#include <new>
#include <type_traits>
#include <memory>

class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t bytes);
    ~ArenaAllocator();

    // Allocate memory from the arena
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    // No deallocation of individual objects, only reset the whole arena
    void reset();

private:
    std::vector<char> m_buffer;
    size_t m_offset;
};

template <typename T>
struct ArenaDeleter {
    void operator()(T* ptr) const noexcept {
        if (ptr) {
            std::destroy_at(ptr);
        }
    }
};

template <typename T>
using ArenaPtr = std::unique_ptr<T, ArenaDeleter<T>>;

template <typename T, typename... Args>
T* arena_new(ArenaAllocator& arena, Args&&... args) {
    void* mem = arena.allocate(sizeof(T), alignof(T));
    return new (mem) T(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
ArenaPtr<T> make_arena_ptr(ArenaAllocator& arena, Args&&... args) {
    return ArenaPtr<T>(arena_new<T>(arena, std::forward<Args>(args)...));
}
