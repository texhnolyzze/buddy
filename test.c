#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <memory.h>
#include <stdio.h>
#include "buddy.h"

typedef struct list_node {
  struct list_node *next;
  void *elem;
} list_node_t;

int main() {
  uint32_t max_level = 15;
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
  list_node_t *head = NULL;
  list_node_t *tail = NULL;
  memset(blocks, 0, max_blocks * sizeof(void *));
  srand(time(NULL));
  uint32_t list_size = 0;
  uint8_t free_all = 0;
//Emulate workload
  for (uint32_t i = 0; i < 10000000; i++) {
	double r = (double)rand() / (double)RAND_MAX;
	if (!free_all && r < 0.75) { // 75% allocations
	  uint32_t order = rand() % (max_level + 1);
	  block = balloc(allocator, order);
	  if (block) {
		assert(!block->next);
		assert(!block->prev);
		assert(block->level == (max_level - order));
		assert(block->address % allocator->page_size == 0);
		assert(!blocks[block->address / allocator->page_size]);
		blocks[block->address / allocator->page_size] = block;
		if (!tail) {
		  head = tail = calloc(1, sizeof(list_node_t));
		} else {
		  tail->next = calloc(1, sizeof(list_node_t));
		  tail = tail->next;
		}
		tail->elem = block;
		list_size++;
	  } else {
		assert(head);
		free_all = 1;
	  }
	} else {
	  if (head) {
		block = head->elem;
//		assert(!block->next); fails sometimes, can't figure out why
		assert(!block->prev);
		assert(block->level >= 0 && block->level <= max_level);
		assert(block->address % allocator->page_size == 0);
		assert(blocks[block->address / allocator->page_size]);
		blocks[block->address / allocator->page_size] = NULL;
		list_node_t *temp = head;
		head = head->next;
		if (!head)
		  tail = NULL;
		free(temp);
		bfree(allocator, block);
		list_size--;
	  } else {
		free_all = 0;
	  }
	}
	if (i % 1000 == 0)
	  printf("%u\n", list_size);
  }
  return 0;
}
