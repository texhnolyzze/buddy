#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "buddy.h"

static inline uint32_t num_bytes_for_bitset(uint32_t num_bits) {
    return num_bits % 8 == 0 ? num_bits / 8 : num_bits / 8 + 1;
}

static inline uint32_t left_child(uint32_t parent_idx) {
    return 2 * parent_idx + 1;
}

static inline uint32_t parent(uint32_t child) {
    return (child - 1) / 2;
}

static inline uint32_t num_nodes_(uint32_t max_level) {
    return (1UL << max_level) - 1;
}

static inline uint32_t block_to_idx(block_t *block, buddy_allocator_t *allocator) {
    return ((1UL << block->level) - 1) + block->address / block_size_at_level(block->level, allocator);
}

static inline uint32_t buddy_idx(block_t *block, buddy_allocator_t *allocator) {
    uint32_t idx = block_to_idx(block, allocator);
    if (idx % 2 == 0)
        return idx - 1;
    return idx + 1;
}

static inline uint8_t mask_idx(uint32_t idx) {
    return 1UL << ((8 - (idx % 8) - 1));
}

static inline uint32_t buddy_used(block_t *block, buddy_allocator_t *allocator) {
    uint32_t buddy = buddy_idx(block, allocator);
    uint32_t byte_pos = buddy / 8;
    return allocator->used_index[byte_pos] & mask_idx(buddy);
}

static inline void mark_used(block_t *block, buddy_allocator_t *allocator) {
    uint32_t idx = block_to_idx(block, allocator);
    uint32_t byte_pos = idx / 8;
    allocator->used_index[byte_pos] = allocator->used_index[byte_pos] | mask_idx(idx);
}

static inline void mark_unused(block_t *block, buddy_allocator_t *allocator) {
    uint32_t idx = block_to_idx(block, allocator);
    uint32_t byte_pos = idx / 8;
    allocator->used_index[byte_pos] = allocator->used_index[byte_pos] & (~0b0U ^ mask_idx(idx));
}

block_t *get_node(uint32_t idx, buddy_allocator_t *allocator) {
    return (void *) allocator->used_index + num_bytes_for_bitset(num_nodes_(allocator->max_level)) +
           idx * sizeof(block_t);
}

static inline uint32_t empty(block_t *block_list) {
    return !block_list;
}

static inline block_t *insert(block_t *block_list, block_t *block) {
    block->prev = NULL;
    block->next = block_list;
    if (block_list)
        block_list->prev = block;
    return block;
}

block_t *delete(block_t *node) {
    block_t *next = node->next;
    if (next)
        next->prev = node->prev;
    if (node->prev)
        node->prev->next = node->next;
    node->prev = NULL;
    node->next = NULL;
    return next;
}

buddy_allocator_t *buddy_allocator_create(uint32_t max_level, uint32_t page_size_order) {
    uint32_t num_nodes = num_nodes_(max_level);
    uint32_t allocator_size =
            sizeof(buddy_allocator_t) + sizeof(void *) * (max_level + 1) + num_bytes_for_bitset(num_nodes) +
            num_nodes * sizeof(block_t);
    buddy_allocator_t *allocator = calloc(1, allocator_size);
    allocator->page_size = 1UL << page_size_order;
    allocator->max_level = max_level;
    allocator->level = (void *) allocator + sizeof(buddy_allocator_t);
    allocator->used_index = (void *) allocator->level + sizeof(void *) * (max_level + 1);
    block_t *root = (void *) (allocator->used_index) + num_bytes_for_bitset(num_nodes);
    allocator->level[0] = root;
    allocator->level[0]->level = 0;
    allocator->level[0]->address = 0;
    printf("Overhead is %f%%\n", ((double) allocator_size / block_size_at_level(0, allocator)) * 100);
    return allocator;
}

static void split(block_t *block, buddy_allocator_t *allocator) {
    uint32_t insert_at_level = block->level + 1;
    uint32_t idx = block_to_idx(block, allocator);
    block_t *left = get_node(left_child(idx), allocator);
    block_t *right = (void *) left + sizeof(block_t);
    left->level = insert_at_level;
    right->level = insert_at_level;
    left->address = block->address;
    right->address = left->address + block_size_at_level(insert_at_level, allocator);
    allocator->level[insert_at_level] = insert(allocator->level[insert_at_level], left);
    allocator->level[insert_at_level] = insert(allocator->level[insert_at_level], right);
}

block_t *balloc(buddy_allocator_t *allocator, uint32_t order) {
    uint32_t level = allocator->max_level - order;
    if (level > allocator->max_level)
        return NULL;
    uint32_t found_at_level = level;
    block_t *block = NULL;
    for (; found_at_level <= allocator->max_level; found_at_level--) {
        if (!empty(allocator->level[found_at_level])) {
            block = allocator->level[found_at_level];
            break;
        }
    }
    if (!block)
        return NULL;
    if (found_at_level == level) {
        mark_used(block, allocator);
        allocator->level[level] = delete(allocator->level[level]);
        return block;
    }
    while (found_at_level < level) {
        block = allocator->level[found_at_level];
        split(block, allocator);
        allocator->level[found_at_level] = delete(allocator->level[found_at_level]);
        mark_used(block, allocator);
        found_at_level++;
    }
    block = allocator->level[level];
    mark_used(block, allocator);
    allocator->level[level] = delete(allocator->level[level]);
    return block;
}

void bfree(buddy_allocator_t *allocator, block_t *block) {
    if (block->level) {
        uint32_t level = block->level;
        do {
            if (buddy_used(block, allocator)) {
                mark_unused(block, allocator);
                allocator->level[level] = insert(allocator->level[level], block);
                return;
            } else {
                mark_unused(block, allocator);
                block_t *buddy = get_node(buddy_idx(block, allocator), allocator);
                if (allocator->level[level] == buddy)
                    allocator->level[level] = NULL;
                else
                    delete(buddy);
                mark_unused(buddy, allocator);
                block = get_node(parent(block_to_idx(block, allocator)), allocator);
            }
            level--;
        } while (level != 0);
        allocator->level[0] = block;
        mark_unused(block, allocator);
    } else {
        allocator->level[0] = block;
        mark_unused(block, allocator);
    }
}
