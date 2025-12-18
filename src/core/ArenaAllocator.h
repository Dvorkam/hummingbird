#pragma once

#include <cstddef>
#include <vector>

class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t bytes);
    ~ArenaAllocator();

    // Allocate memory from the arena
    void* allocate(size_t size);

    // No deallocation of individual objects, only reset the whole arena
    void reset();

private:
    std::vector<char> m_buffer;
    size_t m_offset;
};
