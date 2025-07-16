#ifndef ARENA_PRIVATE_H
#define ARENA_PRIVATE_H

#include "arena.h"

#include <stddef.h>
#include <string.h>
#include <assert.h>

struct arena {
  bool is_aligned;
  uint64_t s_arena, s_block, s_bitmap, max_nodes,
           s_nodes;
  void* memory,
      * ptr;
  struct arena* next;
};

const uint64_t s_word = sizeof(uint64_t);

bool      _arena_is_full(Arena arena, uint64_t s_alloc);
bool      _arena_valid_alloc(Arena* arena, void* ptr);
bool      _arena_set_bitmap(Arena arena, void* ptr, uint64_t blocks, bool full);
bool      _arena_ptr_in_arena(Arena arena, void* ptr);

uint64_t  _arena_size_memory(Arena arena);
uint64_t  _arena_bitmap_size(uint64_t s_arena, uint64_t s_block);
uint64_t  _arena_get_index(Arena* arena, void *ptr);
uint64_t  _arena_bytes_to_blocks(Arena arena, uint64_t bytes);
uint64_t  _arena_blocks_to_offset(Arena arena, uint64_t blocks);

ptrdiff_t _arena_ptr_diff(void* ptr1, void* ptr2);

void*     _arena_ptr_incr(void* ptr, uint64_t bytes);
void*     _arena_ptr_decr(void* ptr, uint64_t bytes);
void*     _arena_get_base_ptr(Arena arena);

// Utils
uint64_t  _arena_utils_next_power_2(uint64_t s);
uint64_t  _arena_utils_bit_count(uint64_t word);
uint64_t  _arena_utils_ceil(double x);

#endif // !ARENA_PRIVATE_H
