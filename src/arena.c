#include "arena_private.h"

// ====================================# PUBLIC #======================================

Arena arena_create(uint64_t s_arena, uint64_t max_nodes) {
  if (s_arena == 0)
    return NULL;

  Arena arena = (Arena)malloc(sizeof(struct arena));
  if (arena == NULL)
    return NULL;
  arena->is_aligned = false; 

  arena->s_arena = _arena_utils_next_power_2(s_arena);
  arena->s_block = 1;

  arena->s_bitmap = _arena_bitmap_size(arena->s_arena, arena->s_block);
  if (arena->s_bitmap == 0) {
    free(arena);
    return NULL;
  } 

  arena->memory = calloc(1, _arena_size_memory(arena));
  if (arena->memory == NULL) {
    free(arena);
    return NULL;
  }
  arena->ptr = _arena_ptr_incr(arena->memory, arena->s_bitmap);

  arena->max_nodes = max_nodes;
  arena->s_nodes = 1;
  arena->next = NULL;

  return arena;
}

Arena arena_create_aligned(uint64_t s_arena, uint64_t s_block, uint64_t max_nodes) {
  if (s_arena == 0)
    return NULL;
  if (s_block < s_word)
    return NULL;

  Arena arena = (Arena)malloc(sizeof(struct arena));
  if (arena == NULL)
    return NULL;
  arena->is_aligned = true; 

  arena->s_arena = _arena_utils_next_power_2(s_arena);
  arena->s_block = _arena_utils_next_power_2(s_block);

  arena->s_bitmap = _arena_bitmap_size(arena->s_arena, arena->s_block);
  if (arena->s_bitmap == 0) {
    free(arena);
    return NULL;
  }

  arena->memory = calloc(1, _arena_size_memory(arena));
  if (arena->memory == NULL) {
    free(arena);
    return NULL;
  }
  arena->ptr = _arena_ptr_incr(arena->memory, arena->s_bitmap);

  arena->max_nodes = max_nodes;
  arena->s_nodes = 1;
  arena->next = NULL;
  
  return arena;
}

void* arena_alloc(Arena arena, uint64_t s_alloc) {
  if (arena == NULL)
    return NULL;
  if (s_alloc == 0)
    return NULL;

  Arena node = arena;
  for (; node->next != NULL; node = node->next);

  if (_arena_is_full(node, s_alloc)) {
    if (arena->s_nodes >= arena->max_nodes)
      return NULL;

    node->next = arena->is_aligned ?
        arena_create_aligned(arena->s_arena, arena->s_block, arena->max_nodes)
      : arena_create(arena->s_arena, arena->max_nodes);
    if (node->next == NULL)
      return NULL;
    arena->s_nodes++;

    return arena_alloc(node->next, s_alloc);
  }
   
  uint64_t* s_ptr = (uint64_t*)node->ptr;
  void* ptr = _arena_ptr_incr(node->ptr, s_word);

  *s_ptr = s_alloc;
  uint64_t blocks = _arena_bytes_to_blocks(node, s_alloc);
  _arena_set_bitmap(node, ptr, blocks, true);
  node->ptr = _arena_ptr_incr(node->ptr, _arena_blocks_to_offset(node, blocks));

  return ptr;
}

void* arena_alloc_array(Arena arena, uint64_t s_obj, uint32_t count) {
  return arena_alloc(arena, (uint64_t)count * s_obj);
}

void* arena_realloc(Arena arena, void* ptr, uint64_t s_realloc) {
  if (arena == NULL)
    return NULL;
  if (ptr == NULL)
    return NULL;

  if (!_arena_ptr_in_arena(arena, ptr))
    return NULL;
  void* new_ptr = arena_alloc(arena, s_realloc);
  if (new_ptr == NULL)
    return NULL;

  uint64_t old_size = *(uint64_t*)_arena_ptr_decr(ptr, s_word);
  if (old_size > s_realloc)
    return NULL;
  memcpy(new_ptr, ptr, s_realloc);

  if (!arena_free(arena, ptr)) {
    (void)arena_free(arena, new_ptr);
    return NULL;
  }

  return new_ptr;
}

char* arena_strdup(Arena arena, char* str) {
  if (str == NULL)
    return NULL;

  uint64_t len = strlen(str) + 1;
  char* copy = (char*)arena_alloc(arena, len);
  if (copy == NULL)
    return NULL;

  memcpy(copy, str, len);

  return copy;
}

bool arena_is_aligned(Arena arena) {
  return arena->is_aligned;
}

uint64_t arena_get_size(Arena arena) {
  return arena->s_arena;
}

uint64_t arena_get_size_bitmap(Arena arena) {
  return arena->s_bitmap;
}

uint64_t arena_get_size_nodes_max(Arena arena) {
  return arena->max_nodes;
}

uint64_t arena_get_size_nodes(Arena arena) {
  return arena->s_nodes;
}

uint64_t arena_get_size_used(Arena arena) {
  if (arena == NULL)
    return 0;
  if (arena->memory == NULL)
    return 0;
  uint64_t counter = 0;
  uint64_t l_bitmap = arena->s_bitmap > s_word ? arena->s_bitmap/s_word : arena->s_bitmap;
  uint64_t* bitmap = (uint64_t*)arena->memory;
  for (uint64_t i = 0; i < l_bitmap; i++)
    counter += _arena_utils_bit_count(bitmap[i]);
  return counter * arena->s_block;
}

bool arena_free(Arena arena, void* ptr) {
  if (arena == NULL)
    return false;
  if (ptr == NULL)
    return false;

  uint64_t* s_ptr = (uint64_t*)(_arena_ptr_decr(ptr, s_word));
  if (*s_ptr == 0)
    return false;

  uint64_t s_alloc = _arena_blocks_to_offset(arena, _arena_bytes_to_blocks(arena, *s_ptr));

  if (!_arena_valid_alloc(&arena, ptr))
    return false;
  memset((void*)s_ptr, 0, s_alloc);
  return _arena_set_bitmap(arena, ptr, s_alloc, false);
}

bool arena_reset(Arena arena) {
  if (arena == NULL)
    return false;
  if (arena->memory == NULL)
    return false;
  memset(arena->memory, 0, arena->s_bitmap);
  memset(_arena_ptr_incr(arena->memory, arena->s_bitmap), 0, arena->s_arena);
  return true;
}

bool arena_destroy(Arena arena) {
  if (arena == NULL)
    return false;
  Arena node = arena;
  while (node != NULL) {
    Arena next = node->next;
    if (node->memory)
      free(node->memory);
    free(node);
    node = next;
  }
  return true;
}

void arena_print(Arena arena, FILE* file) {
  if (arena == NULL)
    return;
  if (file == NULL)
    file = stdout;
  fprintf(file, "Arena %p:\n", (void*)arena);
  fprintf(file, "  aligned:     %s;\n",  arena_is_aligned(arena) ? "true" : "false");
  fprintf(file, "  size bitmap: %zu bytes;\n", arena_get_size_bitmap(arena));
  fprintf(file, "  size block:  %zu bytes;\n", arena->s_block);
  fprintf(file, "  size:        %zu bytes;\n", arena_get_size(arena));
  fprintf(file, "  size used:   %zu bytes;\n", arena_get_size_used(arena));
  fprintf(file, "  max nodes:   %zu;\n", arena_get_size_nodes_max(arena));
  fprintf(file, "  nº nodes:    %zu;\n", arena_get_size_nodes(arena));
}

// ====================================# PRIVATE #======================================

bool _arena_is_full(Arena arena, uint64_t s_alloc) {
  assert(arena != NULL);
  char* stack_ptr = (char*)_arena_ptr_incr(
    arena->ptr,
    arena->is_aligned ? s_word * _arena_utils_ceil((s_alloc + 1)/arena->s_block) : (s_alloc + 1)
  );
  return (ptrdiff_t)(stack_ptr - (char*)arena->ptr) >= (ptrdiff_t)arena->s_arena;
}

bool _arena_ptr_in_arena(Arena arena, void* ptr) {
  assert(arena != NULL);
  assert(ptr != NULL);
  void* base_ptr = _arena_get_base_ptr(arena);
  void* alloc_start = _arena_ptr_decr(ptr, s_word);
  void* alloc_end = _arena_ptr_incr(ptr, *(uint64_t*)alloc_start);
  return (char*)alloc_start >= (char*)(base_ptr) && (char*)alloc_end < ((char*)base_ptr + arena->s_arena);
}

bool _arena_valid_alloc(Arena* arena, void* ptr) {
  assert(arena != NULL);
  if (ptr == NULL) return false; 
  for (Arena node = *arena; node != NULL; node = node->next) {
    if (!_arena_ptr_in_arena(*arena, ptr))
      continue;
    *arena = node;
    return true;
  }
  return false;
}

bool _arena_set_bitmap(Arena arena, void* ptr, uint64_t blocks, bool full) {
  assert(arena != NULL);
  assert(arena->memory != NULL);
  if (ptr == NULL)
    return false;
  if (blocks == 0)
    return false;
  uint64_t i = _arena_get_index(&arena, ptr),
         i_byte = i/8,
         i_bit_offset = i % 8;
  for (uint64_t b = 0; b < blocks; b++) {
    char* byte = &((char*)arena->memory)[i_byte];

    if (full)
      *byte |= (1 << i_bit_offset);
    else
      *byte &= ~(1 << i_bit_offset);

    i_bit_offset++;
    if (i_bit_offset == 8) {
      i_bit_offset = 0;
      i_byte++;
    }
  }
  return true; 
}

uint64_t _arena_size_memory(Arena arena) {
  assert(arena != NULL);
  return arena->s_bitmap + arena->s_arena + s_word * arena->s_arena/arena->s_block;
}

uint64_t _arena_bitmap_size(uint64_t s_arena, uint64_t s_block) {
  return s_arena / (8 * s_block);
}

uint64_t _arena_get_index(Arena* arena, void *ptr) {
  assert(arena != NULL);
  assert(*arena != NULL);
  assert(ptr != NULL);
  assert((*arena)->s_arena > 0);
  assert((*arena)->s_block > 0);
  assert(_arena_ptr_in_arena(*arena, ptr));

  void* base_ptr = _arena_get_base_ptr(*arena);
  ptrdiff_t offset = _arena_ptr_diff(_arena_ptr_decr(ptr, s_word), base_ptr);

  return (*arena)->is_aligned ? offset/(s_word + (*arena)->s_block) : offset;
}

uint64_t _arena_bytes_to_blocks(Arena arena, uint64_t bytes) {
  assert(arena != NULL);
  assert(bytes != 0);
  return arena->is_aligned ? _arena_utils_ceil(((double)(bytes + s_word))/arena->s_block) : (bytes + s_word);
}

uint64_t _arena_blocks_to_offset(Arena arena, uint64_t blocks) {
  assert(arena != NULL);
  assert(blocks != 0);
  return (arena->is_aligned ? (arena->s_block + s_word) : 1) * blocks;
}

bool _arena_ptr_equals(void* ptr1, void* ptr2) {
  return ptr1 == ptr2;
}

ptrdiff_t _arena_ptr_diff(void* ptr1, void* ptr2) {
  assert(ptr1 != NULL && ptr2 != NULL);
  return (ptrdiff_t)((char*)ptr1 - (char*)ptr2);
}

void* _arena_ptr_incr(void* ptr, uint64_t bytes) {
  assert(ptr != NULL);
  return (char*)ptr + bytes;
}

void* _arena_ptr_decr(void* ptr, uint64_t bytes) {
  assert(ptr != NULL);
  return (char*)ptr - bytes;
}

void* _arena_get_base_ptr(Arena arena) {
  return _arena_ptr_incr(arena->memory, arena->s_bitmap);
}

// Utils

uint64_t _arena_utils_next_power_2(uint64_t s) {
  if (s == 0)
    return 1;
  s--;
  s |= s >> 1;
  s |= s >> 2;
  s |= s >> 4;
  s |= s >> 8;
  s |= s >> 16;
  if (s_word > 4)
    s |= s >> 32;
  return s + 1;
}

uint64_t _arena_utils_bit_count(uint64_t word) {
  uint64_t count = 0;
  for (; word > 0; word &= (word-1), count++);
  return count;
}

uint64_t _arena_utils_ceil(double x) {
  return (uint64_t)x + ((x > (double)((uint64_t)x)) ? 1 : 0);
}
