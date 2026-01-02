#include "core/ArenaAllocator.h"

#include <stdexcept>

namespace {
size_t align_up(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

size_t padding_for_alignment(const std::vector<char>& buffer, size_t offset, size_t alignment) {
    size_t current = reinterpret_cast<size_t>(buffer.data() + offset);
    size_t aligned = align_up(current, alignment);
    return aligned - current;
}
}  // namespace

ArenaAllocator::ArenaAllocator(size_t bytes) : m_offset(0) {
    m_buffer.resize(bytes);
}

ArenaAllocator::~ArenaAllocator() {
    // m_buffer will be deallocated automatically
}

void* ArenaAllocator::allocate(size_t size, size_t alignment) {
    size_t padding = padding_for_alignment(m_buffer, m_offset, alignment);
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
