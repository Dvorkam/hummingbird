#include "core/ArenaAllocator.h"
#include <stdexcept>

ArenaAllocator::ArenaAllocator(size_t bytes) : m_offset(0) {
    m_buffer.resize(bytes);
}

ArenaAllocator::~ArenaAllocator() {
    // m_buffer will be deallocated automatically
}

void* ArenaAllocator::allocate(size_t size) {
    if (m_offset + size > m_buffer.size()) {
        throw std::bad_alloc();
    }
    void* ptr = &m_buffer[m_offset];
    m_offset += size;
    return ptr;
}

void ArenaAllocator::reset() {
    m_offset = 0;
}
