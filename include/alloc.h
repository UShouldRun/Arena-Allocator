#ifndef __ALLOC_H__
#define __ALLOC_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "utils.h"

/**
 * @file alloc.h
 * @brief Memory Allocation Interface (Arena & Pool)
 *
 * * 1. Arena (Linear/Stack Allocator):
 * - What: A contiguous buffer that grows by appending nodes. No individual frees.
 * - When: Use for "per-frame" or "per-task" lifetimes where all memory is released at once.
 * - Time Complexity: O(1) for alloc, O(1) for reset.
 * - Space Complexity: O(N) where N is the total requested size (plus 8-byte headers).
 *
 * * 2. Pool (Best-Fit Block Allocator):
 * - What: Segregated storage that divides memory into fixed-size blocks.
 * - When: Use for long-lived objects of varying sizes where you need to free and reuse memory.
 * - Time Complexity: O(F) for alloc/free (F = free list length). O(1) for reset.
 * - Space Complexity: O(N) with metadata overhead stored in an internal Arena.
 */

#define KB(x) ((u64)(x) << 10)
#define MB(x) ((u64)(x) << 20)
#define GB(x) ((u64)(x) << 30)

typedef struct arena Arena;

Arena* arena_create             (const u64 s_arena, const u64 max_nodes);

void*  arena_alloc              (Arena* arena, const u64 s_alloc);
void*  arena_alloc_array        (Arena* arena, const u64 s_obj, const u32 count);
void*  arena_realloc            (Arena* arena, void* ptr, const u64 s_realloc);

char*  arena_strdup             (Arena* arena, char* str);

void   arena_print              (Arena* arena, FILE* file);

bool   arena_reset              (Arena* arena);
bool   arena_destroy            (Arena* arena);

u64    arena_get_size           (Arena* arena);
u64    arena_get_size_nodes_max (Arena* arena);
u64    arena_get_size_nodes     (Arena* arena);
u64    arena_get_size_used      (Arena* arena);

typedef struct pool Pool;

Pool*  pool_create              (const u64 s_arena, const u64 s_block, const u64 max_nodes);

void*  pool_alloc               (Pool* pool, const u64 s_alloc);
void*  pool_alloc_array         (Pool* pool, const u64 s_obj, const u32 count);
void*  pool_realloc             (Pool* pool, void* ptr, const u64 s_realloc);

char*  pool_strdup              (Pool* pool, char* str);

void   pool_print               (Pool* pool, FILE* file);

bool   pool_free                (Pool* pool, void* ptr);
bool   pool_reset               (Pool* pool);
bool   pool_destroy             (Pool* pool);

u64    pool_get_size            (Pool* pool);
u64    pool_get_size_nodes_max  (Pool* pool);
u64    pool_get_size_nodes      (Pool* pool);
u64    pool_get_size_used       (Pool* pool);

#endif // !__ALLOC_H__
