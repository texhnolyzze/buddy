#include <sys/types.h>
#include <stdint.h>

#ifndef BUDDY_BUDDY_H
#define BUDDY_BUDDY_H

#endif //BUDDY_BUDDY_H

typedef struct block {
    uint8_t level;
    uint32_t address;
    struct block *next;
    struct block *prev;
} block_t;

typedef struct buddy_allocator {
    uint32_t page_size;
    uint8_t max_level;
    block_t **level;
    uint8_t *used_index;
} buddy_allocator_t;

static inline uint32_t block_size_at_level(uint32_t level, buddy_allocator_t *allocator) {
    return (1UL << (allocator->max_level - level)) * allocator->page_size;
}

static inline uint32_t block_size(block_t *block, buddy_allocator_t *allocator) {
    return block_size_at_level(block->level, allocator);
}

buddy_allocator_t *buddy_allocator_create(uint32_t max_level, uint32_t page_size_order);
block_t *balloc(buddy_allocator_t *allocator, uint32_t order);
void bfree(buddy_allocator_t *allocator, block_t *block);