#ifndef __ALLOC_PRIVATE_H__
#define __ALLOC_PRIVATE_H__

#include "alloc.h"

#include <stddef.h>
#include <string.h>
#include <assert.h>

/**
 * @file alloc_private.h
 * @brief Implementation Details & Internal Structures
 *
 * * Implementation Strategy:
 * - Arena: Uses a simple pointer-increment logic. Each allocation is preceded by a u64 size header.
 * - Pool: Implemented using a "Best-Fit" strategy. The `free_list` is strictly maintained 
 * in ascending order of size (s_blocks).
 *
 * - Metadata Management: To avoid system heap fragmentation, the Pool's `FreeRegion` descriptors 
 * are allocated from an internal Arena (`fl_arena`).
 *
 * - Why: This hybrid approach ensures that the "meta-memory" for the free list is just as 
 * stable and contiguous as the data memory itself, preventing "fragmentation of the 
 * fragmentation manager."
 */

struct arena {
  u64 
    s_arena,
    s_nodes,
    max_nodes;

  void* memory,
      * ptr;

  struct arena* next;
};

typedef struct free_region FreeRegion;

struct pool {
  u64 
    s_pool,
    s_block,
    s_nodes,
    max_nodes;

  Arena* fl_arena;
  FreeRegion* free_list;

  void* memory;

  struct pool* next;
};

struct free_region {
  u64 
    start_block,
    s_blocks;
  struct free_region* prev, *next;
};

#define S_WORD sizeof(u64)

// Arena

bool                    __arena_is_full             (Arena* arena, const u64 s_alloc);
bool                    __arena_valid_alloc         (Arena** arena, const void* ptr);
bool                    __arena_ptr_in_arena        (const Arena* arena, const void* ptr);

static inline u64       __arena_size_memory         (const Arena* arena);

u64                     __arena_get_index           (Arena* arena, const void *ptr);
static inline void*     __arena_get_base_ptr        (const Arena* arena);

// Pool

bool                    __pool_valid_alloc          (Pool** pool, const void* ptr);
bool                    __pool_ptr_in_pool          (const Pool* pool, const void* ptr);

bool                    __pool_free_region_find     (Pool* pool, const u64 blocks, u64* index);
void                    __pool_free_region_append   (Pool* pool, const u64 s_blocks, const u64 start_block);
bool                    __pool_free_region_update   (Pool* pool, const u64 index, const u64 blocks);

static inline u64       __pool_region_end           (const FreeRegion* region);
void                    __pool_unlink_region        (Pool* pool, FreeRegion* regiona);
void                    __pool_insert_region_sorted (Pool* pool, FreeRegion* region);

static inline u64       __pool_size_memory          (const Pool* pool);
static inline u64       __pool_bytes_to_block       (const Pool* pool, const u64 bytes);

u64                     __pool_get_index            (const Pool* pool, const void *ptr);
static inline void*     __pool_get_base_ptr         (const Pool* pool);

// Alloc Utils

static inline void*     __alloc_utils_ptr_incr      (const void* ptr, const u64 bytes);
static inline void*     __alloc_utils_ptr_decr      (const void* ptr, const u64 bytes);
static inline ptrdiff_t __alloc_utils_ptr_diff      (const void* ptr1, const void* ptr2);

u64                     __alloc_utils_next_power_2  (u64 s);
static inline u64       __alloc_utils_ceil          (const f64 x);

static inline u64       __alloc_utils_max           (const u64 a, const u64 b);
static inline u64       __alloc_utils_min           (const u64 a, const u64 b);

static inline void      __alloc_utils_swap          (u64* a, u64* b);

#endif // !__ALLOC_PRIVATE_H__
