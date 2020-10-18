#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "buddy.h"

int main() {
    uint32_t max_level = 4;
    uint32_t page_size_order = 5;
    uint32_t max_blocks = 1UL << max_level;
    buddy_allocator_t *allocator = buddy_allocator_create(max_level, page_size_order);
    block_t **blocks = calloc(max_blocks, sizeof(void *));
    for (uint32_t i = 0; i < max_blocks; i++) {
        block_t *block = balloc(allocator, 0);
        blocks[i] = block;
        assert(block);
    }
    for (uint32_t i = 0; i <= max_level; i++) {
        assert(balloc(allocator, i) == NULL);
    }
    for (uint32_t i = 0; i < max_blocks; i++) {
        bfree(allocator, blocks[i]);
    }
    block_t *block = balloc(allocator, max_level);
    assert(block);
    assert(block->address == 0);
    assert(block->level == 0);
    assert(!balloc(allocator, max_level));
    bfree(allocator, block);
    block = balloc(allocator, 1);
    assert(block);
    bfree(allocator, block);
    block = balloc(allocator, max_level);
    assert(block);
    assert(block->address == 0);
    bfree(allocator, block);
    uint32_t insert_pos = 0;
    srand(time(NULL));
//  Test for segfaults
    for (uint32_t i = 0; i < 100000; i++) {
        if (rand() % 2) {
            block = balloc(allocator, i % (max_level + 1));
            if (block) {
                assert(block->level >= 0 && block->level <= max_level);
                assert(!block->next);
                assert(!block->prev);
                assert(block->address % allocator->page_size == 0);
                blocks[insert_pos] = block;
                insert_pos = (insert_pos + 1) % max_blocks;
            }
        } else {
            for (uint32_t j = 0; j < max_blocks; j++) {
                if (blocks[j]) {
                    block = blocks[j];
                    bfree(allocator, block);
                    blocks[j] = NULL;
                    break;
                }
            }
        }
    }
    return 0;
}
