#include "core/ArenaAllocator.h"

#include <algorithm>
#include <ostream>

#include "core/utils/Log.h"

namespace {
size_t align_up(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

size_t padding_for_alignment(const std::vector<char>& buffer, size_t offset, size_t alignment) {
    size_t current = reinterpret_cast<size_t>(buffer.data() + offset);
    size_t aligned = align_up(current, alignment);
    return aligned - current;
}

size_t required_capacity(size_t size, size_t alignment) {
    return size + alignment;
}

}  // namespace

namespace Hummingbird::Core {

ArenaAllocator::ArenaAllocator(size_t bytes, size_t max_blocks)
    : m_default_block_size(bytes), m_max_blocks(max_blocks) {
    m_blocks.push_back(Block{std::vector<char>(bytes), 0});
}

ArenaAllocator::~ArenaAllocator() {
    // m_blocks will be deallocated automatically
}

void* ArenaAllocator::allocate(size_t size, size_t alignment) {
    if (m_failed) {
        return nullptr;
    }
    if (m_blocks.empty()) {
        m_blocks.push_back(Block{std::vector<char>(m_default_block_size), 0});
    }

    Block* block = &m_blocks.back();
    size_t padding = padding_for_alignment(block->buffer, block->offset, alignment);
    if (block->offset + padding + size > block->buffer.size()) {
        if (m_max_blocks > 0 && m_blocks.size() >= m_max_blocks) {
            HB_LOG_ERROR("[arena] budget exceeded: request=" << size << " align=" << alignment << " blocks="
                                                             << m_blocks.size() << " max_blocks=" << m_max_blocks);
            m_failed = true;
            return nullptr;
        }
        const size_t new_size = std::max(m_default_block_size, required_capacity(size, alignment));
        HB_LOG_DEBUG("[arena] growing: request=" << size << " align=" << alignment << " new_block=" << new_size);
        m_blocks.push_back(Block{std::vector<char>(new_size), 0});
        block = &m_blocks.back();
        padding = padding_for_alignment(block->buffer, block->offset, alignment);
    }

    block->offset += padding;
    void* ptr = &block->buffer[block->offset];
    block->offset += size;
    return ptr;
}

void ArenaAllocator::reset() {
    m_failed = false;
    if (m_blocks.empty()) {
        return;
    }
    if (m_blocks.size() > 1) {
        m_blocks.resize(1);
        m_blocks[0].buffer.resize(m_default_block_size);
    }
    m_blocks[0].offset = 0;
}

}  // namespace Hummingbird::Core
