#include "core/ArenaAllocator.h"

#include <stdexcept>

ArenaAllocator::ArenaAllocator(size_t bytes) : m_offset(0) {
    m_buffer.resize(bytes);
}

ArenaAllocator::~ArenaAllocator() {
    // m_buffer will be deallocated automatically
}

void* ArenaAllocator::allocate(size_t size, size_t alignment) {
    size_t current = reinterpret_cast<size_t>(&m_buffer[m_offset]);
    size_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned - current;
    if (m_offset + padding + size > m_buffer.size()) {
        throw std::bad_alloc();
    }
    m_offset += padding;
    void* ptr = &m_buffer[m_offset];
    m_offset += size;
    return ptr;
}

void ArenaAllocator::reset() {
    m_offset = 0;
}
