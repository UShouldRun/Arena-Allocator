#ifndef ARENA_H
#define ARENA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct arena *Arena;

Arena    arena_create(uint64_t s_arena, uint64_t max_nodes);
Arena    arena_create_aligned(uint64_t s_arena, uint64_t s_block, uint64_t max_nodes);

void*    arena_alloc(Arena arena, uint64_t s_alloc);
void*    arena_alloc_array(Arena arena, uint64_t s_obj, uint32_t count);
void*    arena_realloc(Arena arena, void* ptr, uint64_t s_realloc);

char*    arena_strdup(Arena arena, char* str);

void     arena_print(Arena arena, FILE* file);

bool     arena_is_aligned(Arena arena);
bool     arena_free(Arena arena, void* ptr);
bool     arena_reset(Arena arena);
bool     arena_destroy(Arena arena);

uint64_t arena_get_size(Arena arena);
uint64_t arena_get_size_bitmap(Arena arena);
uint64_t arena_get_size_nodes_max(Arena arena);
uint64_t arena_get_size_nodes(Arena arena);
uint64_t arena_get_size_used(Arena arena);

#endif // !ARENA_H
