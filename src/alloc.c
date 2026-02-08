#include "alloc_private.h"

/**
 * CRITICAL IMPLEMENTATION NOTES:
 *
 * * __pool_free_region_find:
 * Implements Best-Fit. Since the list is sorted ASCENDING, the first region found 
 * that satisfies 'blocks' is the smallest possible fit. After splitting, the 
 * remaining fragment is shrunk and "bubbled up" toward the head of the list 
 * using a swap-based sort to maintain the ascending invariant.
 *
 * * __pool_free_region_update:
 * Handles deallocation. It ignores the list order to perform a linear scan 
 * for physical adjacency (left/right neighbors). This allows us to merge 
 * contiguous free blocks back into larger ones, fighting external fragmentation.
 * Once merged, the region is re-inserted into the list based on its new size.
 */

// ====================================# PUBLIC #======================================

// Public Arena

Arena* arena_create(const u64 s_arena, const u64 max_nodes) {
  if (s_arena == 0)
    return NULL;

  Arena* arena = (Arena*)malloc(sizeof(struct arena));
  if (arena == NULL)
    return NULL;

  arena->s_arena = __alloc_utils_next_power_2(s_arena);

  arena->memory = calloc(1, __arena_size_memory(arena));
  if (arena->memory == NULL) {
    free(arena);
    return NULL;
  }

  arena->ptr       = __arena_get_base_ptr(arena);
  arena->max_nodes = max_nodes;
  arena->s_nodes   = 1;
  arena->next      = NULL;

  return arena;
}

void* arena_alloc(Arena* arena, const u64 s_alloc) {
  if (arena == NULL)
    return NULL;

  if (s_alloc == 0)
    return NULL;

  Arena* node = arena;
  for (
    ;
    node->next != NULL && __arena_is_full(node, s_alloc);
    node = node->next
  );

  if (node->next == NULL && __arena_is_full(node, s_alloc)) {
    if (arena->s_nodes >= arena->max_nodes)
      return NULL;

    node->next = arena_create(arena->s_arena, arena->max_nodes);

    if (node->next == NULL)
      return NULL;

    arena->s_nodes++;

    return arena_alloc(node->next, s_alloc);
  }
   
  u64* s_ptr = (u64*)node->ptr;
  void* ptr = __alloc_utils_ptr_incr(node->ptr, S_WORD);

  *s_ptr = s_alloc;

  node->ptr = __alloc_utils_ptr_incr(ptr, s_alloc);

  return ptr;
}

void* arena_alloc_array(Arena* arena, const u64 s_obj, const u32 count) {
  return arena_alloc(arena, (u64)count * s_obj);
}

void* arena_realloc(Arena* arena, void* ptr, const u64 s_realloc) {
  if (arena == NULL)
    return NULL;

  if (ptr == NULL)
    return NULL;

  if (!__arena_ptr_in_arena(arena, ptr))
    return NULL;

  void* new_ptr = arena_alloc(arena, s_realloc);
  if (new_ptr == NULL)
    return NULL;

  u64 old_size = *(u64*)__alloc_utils_ptr_decr(ptr, S_WORD);

  memcpy(new_ptr, ptr, __alloc_utils_min(old_size, s_realloc));

  return new_ptr;
}

char* arena_strdup(Arena* arena, char* str) {
  if (str == NULL)
    return NULL;

  u64 len = strlen(str) + 1;
  char* copy = (char*)arena_alloc(arena, len);
  if (copy == NULL)
    return NULL;

  memcpy(copy, str, len);

  return copy;
}

u64 arena_get_size(Arena* arena) {
  return arena->s_arena;
}

u64 arena_get_size_nodes_max(Arena* arena) {
  return arena->max_nodes;
}

u64 arena_get_size_nodes(Arena* arena) {
  return arena->s_nodes;
}

u64 arena_get_size_used(Arena* arena) {
  if (arena == NULL)
    return 0;

  if (arena->memory == NULL)
    return 0;

  return __alloc_utils_ptr_diff(arena->ptr, arena->memory);
}

bool arena_reset(Arena* arena) {
  if (arena == NULL)
    return false;

  if (arena->memory == NULL)
    return false;

  memset(arena->memory, 0, arena->s_arena);
  arena->ptr = arena->memory;

  return true;
}

bool arena_destroy(Arena* arena) {
  if (arena == NULL)
    return false;

  Arena* node = arena;
  while (node != NULL) {
    Arena* next = node->next;

    if (node->memory)
      free(node->memory);
    free(node);

    node = next;
  }

  return true;
}

void arena_print(Arena* arena, FILE* file) {
  if (arena == NULL)
    return;

  if (file == NULL)
    file = stdout;

  fprintf(file, "Arena* %p: {\n",              (void*)arena);
  fprintf(file, "  size:        %zu bytes;\n", arena_get_size(arena));
  fprintf(file, "  size used:   %zu bytes;\n", arena_get_size_used(arena));
  fprintf(file, "  max nodes:   %zu;\n",       arena_get_size_nodes_max(arena));
  fprintf(file, "  nº nodes:    %zu;\n}",      arena_get_size_nodes(arena));
}

// Public Pool

Pool* pool_create(const u64 s_pool, const u64 s_block, const u64 max_nodes) {
  if (s_pool == 0)
    return NULL;

  if (s_block < S_WORD)
    return NULL;

  Pool* pool = (Pool*)malloc(sizeof(struct pool));
  if (pool == NULL)
    return NULL;

  pool->s_pool  = __alloc_utils_next_power_2(s_pool);
  pool->s_block = __alloc_utils_next_power_2(s_block);

  const u64 s_fl_arena = __alloc_utils_min(MB(10), __alloc_utils_max(KB(1), pool->s_pool / 100));
  pool->fl_arena = arena_create(s_fl_arena, 5);

  __pool_free_region_append(pool, pool->s_pool / pool->s_block, 0);

  pool->memory = calloc(1, __pool_size_memory(pool));
  if (pool->memory == NULL) {
    free(pool);
    return NULL;
  }

  pool->max_nodes = max_nodes;
  pool->s_nodes = 1;
  pool->next = NULL;
  
  return pool;
}

void* pool_alloc(Pool* pool, const u64 s_alloc) {
  if (pool == NULL || s_alloc == 0)
    return NULL;
  
  u64 blocks = __pool_bytes_to_block(pool, s_alloc);
  
  Pool* node = pool;
  u64 block_index;
  
  for (; true; node = node->next) {
    if (__pool_free_region_find(node, blocks, &block_index))
      break;
    
    if (node->next != NULL)
      continue;

    if (pool->s_nodes >= pool->max_nodes)
      return NULL;
    
    node->next = pool_create(pool->s_pool, pool->s_block, pool->max_nodes);
    if (node->next == NULL)
      return NULL;
    
    pool->s_nodes++;
    node = node->next;
    
    block_index = 0;
    break;
  }
  
  if (node == NULL)
    return NULL;
  
  void* base = __pool_get_base_ptr(node);
  void* ptr  = __alloc_utils_ptr_incr(
    base,
    block_index * (S_WORD + node->s_block) + S_WORD
  );

  u64* s_ptr = (u64*)__alloc_utils_ptr_decr(ptr, S_WORD);
  *s_ptr = s_alloc;
  
  return ptr;
}

void* pool_alloc_array(Pool* pool, const u64 s_obj, const u32 count) {
  return pool_alloc(pool, (u64)count * s_obj);
}

void* pool_realloc(Pool* pool, void* ptr, const u64 s_realloc) {
  if (pool == NULL)
    return NULL;
  
  if (ptr == NULL)
    return NULL;

  if (!__pool_ptr_in_pool(pool, ptr))
    return NULL;

  void* new_ptr = pool_alloc(pool, s_realloc);
  if (new_ptr == NULL)
    return NULL;

  u64* s_ptr = __alloc_utils_ptr_decr(ptr, S_WORD);

  u64 old_size = *s_ptr;
  if (old_size > s_realloc)
    return NULL;

  memcpy(new_ptr, ptr, s_realloc);

  if (!pool_free(pool, ptr)) {
    (void)pool_free(pool, new_ptr);
    return NULL;
  }

  return new_ptr;
}

char* pool_strdup(Pool* pool, char* str) {
  if (str == NULL)
    return NULL;

  u64 len = strlen(str) + 1;
  char* copy = (char*)pool_alloc(pool, len);
  if (copy == NULL)
    return NULL;

  memcpy(copy, str, len);

  return copy;
}

u64 pool_get_size(Pool* pool) {
  return pool->s_pool;
}

u64 pool_get_size_nodes_max(Pool* pool) {
  return pool->max_nodes;
}

u64 pool_get_size_nodes(Pool* pool) {
  return pool->s_nodes;
}

u64 pool_get_size_used(Pool* pool) {
  if (pool == NULL)
    return 0;

  if (pool->memory == NULL)
    return 0;

  u64 total = 0;

  for (Pool* node = pool; node != NULL; node = node->next) {
    u64 count = 0;

    for (
      FreeRegion* region = node->free_list;
      region != NULL;
      count += region->s_blocks, region = region->next
    );

    // doing it here to avoid overflow with multiplications
    total += node->s_pool - count * pool->s_block;
  }

  return total;
}

bool pool_free(Pool* pool, void* ptr) {
  if (pool == NULL)
    return false;

  if (ptr == NULL)
    return false;

  u64* s_ptr = __alloc_utils_ptr_decr(ptr, S_WORD);

  u64 s_alloc = *s_ptr;
  if (s_alloc == 0)
    return false;

  if (!__pool_valid_alloc(&pool, ptr))
    return false;

  memset((void*)s_ptr, 0, S_WORD + s_alloc);

  const u64 
    index  = __pool_get_index(pool, ptr),
    blocks = __pool_bytes_to_block(pool, s_alloc);

  return __pool_free_region_update(pool, index, blocks);
}

bool pool_reset(Pool* pool) {
  if (pool == NULL)
    return false;

  if (pool->memory == NULL)
    return false;

  for (Pool* node = pool; pool != NULL; pool = pool->next) {
    arena_reset(node->fl_arena);

    node->free_list = (FreeRegion*)arena_alloc(pool->fl_arena, sizeof(struct free_region));
    assert(pool->free_list != NULL);

    *node->free_list = (FreeRegion){
      .start_block = 0,
      .s_blocks    = node->s_pool / node->s_block,
      .prev        = NULL,
      .next        = NULL
    };

    memset(node->memory, 0, __pool_size_memory(node));
  }

  return true;
}

bool pool_destroy(Pool* pool) {
  if (pool == NULL)
    return false;

  Pool* node = pool;
  while (node != NULL) {
    Pool* next = node->next;

    if (node->memory)
      free(node->memory);
    arena_destroy(node->fl_arena);
    free(node);

    node = next;
  }

  return true;
}

void pool_print(Pool* pool, FILE* file) {
  if (pool == NULL)
    return;

  if (file == NULL)
    file = stdout;

  fprintf(file, "Pool* %p: {\n",                     (void*)pool);
  fprintf(file, "  size block:  %zu bytes;\n",       pool->s_block);
  fprintf(file, "  size:        %zu bytes/node;\n",  pool_get_size(pool));
  fprintf(file, "  size used:   %zu bytes total;\n", pool_get_size_used(pool));
  fprintf(file, "  max nodes:   %zu;\n",             pool_get_size_nodes_max(pool));
  fprintf(file, "  nº nodes:    %zu;\n}",            pool_get_size_nodes(pool));
}

// ====================================# PRIVATE #======================================

// Private Arena

bool __arena_is_full(Arena* arena, const u64 s_alloc) {
  assert(arena != NULL);
  void* stack_ptr = __alloc_utils_ptr_incr(arena->ptr, S_WORD + s_alloc);
  return __alloc_utils_ptr_diff(stack_ptr, arena->ptr) >= (ptrdiff_t)arena->s_arena;
}

bool __arena_ptr_in_arena(const Arena* arena, const void* ptr) {
  assert(arena != NULL);
  assert(ptr != NULL);

  const u8
    *arena_base_ptr = __arena_get_base_ptr(arena),
    *arena_end_ptr  = __alloc_utils_ptr_incr(arena_base_ptr, arena->s_arena),
    *alloc_start    = __alloc_utils_ptr_decr(ptr, S_WORD),
    *alloc_end      = __alloc_utils_ptr_incr(ptr, *(u64*)alloc_start);

  return 
    alloc_start >= arena_base_ptr && 
    alloc_end   <  arena_end_ptr;
}

bool __arena_valid_alloc(Arena** arena, const void* ptr) {
  assert(arena != NULL);

  if (ptr == NULL)
    return false; 

  for (Arena* node = *arena; node != NULL; node = node->next) {
    if (!__arena_ptr_in_arena(*arena, ptr))
      continue;

    *arena = node;
    return true;
  }

  return false;
}

static inline u64 __arena_size_memory(const Arena* arena) {
  assert(arena != NULL);
  return arena->s_arena;
}

u64 __arena_get_index(Arena* arena, const void *ptr) {
  assert(arena != NULL);
  assert(ptr != NULL);
  assert(arena->s_arena > 0);
  assert(__arena_ptr_in_arena(arena, ptr));

  void* base_ptr   = __arena_get_base_ptr(arena);
  ptrdiff_t offset = __alloc_utils_ptr_diff(ptr, base_ptr);

  return offset;
}

static inline void* __arena_get_base_ptr(const Arena* arena) {
  return arena->memory;
}

// Private Pool

bool __pool_valid_alloc(Pool** pool, const void* ptr) {
  assert(pool != NULL);

  if (ptr == NULL)
    return false; 

  for (Pool* node = *pool; node != NULL; node = node->next) {
    if (!__pool_ptr_in_pool(*pool, ptr))
      continue;

    *pool = node;
    return true;
  }

  return false;
}

bool __pool_ptr_in_pool(const Pool* pool, const void* ptr) {
  assert(pool != NULL);
  assert(ptr != NULL);

  const u64 s_alloc = *(u64*)__alloc_utils_ptr_decr(ptr, S_WORD);

  const u8
    *pool_base_ptr = __pool_get_base_ptr(pool),
    *pool_end_ptr  = __alloc_utils_ptr_incr(pool_base_ptr, pool->s_pool),
    *alloc_start   = ptr,
    *alloc_end     = __alloc_utils_ptr_incr(ptr, s_alloc);

  return 
    alloc_start >= pool_base_ptr && 
    alloc_end   <  pool_end_ptr;
}

bool __pool_free_region_find(Pool* pool, const u64 blocks, u64* index) {
  assert(pool != NULL);
  
  if (blocks == 0)
    return false;

  for (
    FreeRegion* region = pool->free_list; 
    region != NULL;
    region = region->next
  ) {
    if (region->s_blocks < blocks)
      continue;

    *index = region->start_block;
  
    region->s_blocks -= blocks;
    region->start_block += blocks;

    if (region->s_blocks == 0) {
      __pool_unlink_region(pool, region);
    } else {
      for (
        ;
        region->prev != NULL &&
        region->s_blocks < region->prev->s_blocks;
        region = region->prev
      ) {
        __alloc_utils_swap(&region->start_block, &region->prev->start_block);
        __alloc_utils_swap(&region->s_blocks, &region->prev->s_blocks);
      }
    } 

    return true;
  }

  return false;
}

void __pool_free_region_append(Pool* pool, const u64 s_blocks, const u64 start_block) {
  assert(pool != NULL);
  assert(s_blocks > 0);

  FreeRegion
    **node = &pool->free_list,
    *prev  = NULL;
  for (
    ;
    *node != NULL;
    prev  = *node, *node = (*node)->next
  );

  *node = (FreeRegion*)arena_alloc(pool->fl_arena, sizeof(struct free_region));
  assert(*node != NULL);

  **node = (FreeRegion) {
    .start_block = start_block,
    .s_blocks    = s_blocks,
    .prev        = prev,
    .next        = NULL
  };
}

bool __pool_free_region_update(Pool* pool, u64 index, u64 blocks) {
  assert(pool);
  assert(blocks > 0);

  FreeRegion* left  = NULL;
  FreeRegion* right = NULL;

  // 1. Find adjacent regions by scanning the size-sorted list
  // We look for any region that ends where our new block starts (left)
  // or starts where our new block ends (right).
  for (FreeRegion* r = pool->free_list; r; r = r->next) {
    if (__pool_region_end(r) == index)
      left = r;
    else if (r->start_block == index + blocks)
      right = r;
    
    if (left && right)
      break;
  }

  // 2. Handle Merging
  if (left && right) {
    // Case: [Left][Freed][Right] -> One large region
    __pool_unlink_region(pool, left);
    __pool_unlink_region(pool, right);

    left->s_blocks += blocks + right->s_blocks;
    
    __pool_insert_region_sorted(pool, left);

    return true;
  }

  if (left) {
    // Case: [Left][Freed]
    __pool_unlink_region(pool, left);
    left->s_blocks += blocks;
    __pool_insert_region_sorted(pool, left);

    return true;
  }

  if (right) {
    // Case: [Freed][Right]
    __pool_unlink_region(pool, right);
    right->start_block = index;
    right->s_blocks += blocks;
    __pool_insert_region_sorted(pool, right);

    return true;
  }

  // 3. No neighbors: Create a brand new region
  FreeRegion* region = (FreeRegion*)arena_alloc(pool->fl_arena, sizeof(struct free_region));
  assert(region);

  *region = (FreeRegion){
    .start_block = index,
    .s_blocks    = blocks,
    .prev        = NULL,
    .next        = NULL
  };

  __pool_insert_region_sorted(pool, region);
  return true;
}

static inline u64 __pool_region_end(const FreeRegion* region) {
  return region->start_block + region->s_blocks;
}

void __pool_unlink_region(Pool* pool, FreeRegion* region) {
  if (region->prev)
    region->prev->next = region->next;
  else
    pool->free_list = region->next;

  if (region->next)
    region->next->prev = region->prev;

  region->prev = region->next = NULL;
}

void __pool_insert_region_sorted(Pool* pool, FreeRegion* region) {
  FreeRegion* curr = pool->free_list;
  FreeRegion* prev = NULL;

  while (curr && curr->s_blocks <= region->s_blocks) {
    prev = curr;
    curr = curr->next;
  }

  region->prev = prev;
  region->next = curr;

  if (prev)
    prev->next = region;
  else
    pool->free_list = region;

  if (curr)
    curr->prev = region;
}

static inline u64 __pool_size_memory(const Pool* pool) {
  assert(pool != NULL);
  // memory for allocations + memory for allocation size header
  return pool->s_pool + S_WORD * pool->s_pool / pool->s_block;
}

static inline u64 __pool_bytes_to_block(const Pool* pool, const u64 bytes) {
  return __alloc_utils_ceil((f64)bytes / pool->s_block);
}

u64 __pool_get_index(const Pool* pool, const void *ptr) {
  assert(pool != NULL);
  assert(ptr != NULL);
  assert(pool->s_pool > 0);
  assert(__pool_ptr_in_pool(pool, ptr));

  void* base_ptr   = __pool_get_base_ptr(pool);
  ptrdiff_t offset = __alloc_utils_ptr_diff(ptr, base_ptr);

  // a block really is the header with the size + size of the block
  return offset / (S_WORD + pool->s_block);
}

static inline void* __pool_get_base_ptr(const Pool* pool) {
  return pool->memory;
}

// Utils

static inline void* __alloc_utils_ptr_incr(const void* ptr, const u64 bytes) {
  assert(ptr != NULL);
  return (u8*)ptr + bytes;
}

static inline void* __alloc_utils_ptr_decr(const void* ptr, const u64 bytes) {
  assert(ptr != NULL);
  return (u8*)ptr - bytes;
}

static inline ptrdiff_t __alloc_utils_ptr_diff(const void* ptr1, const void* ptr2) {
  assert(ptr1 != NULL && ptr2 != NULL);
  return (ptrdiff_t)((u8*)ptr1 - (u8*)ptr2);
}

u64 __alloc_utils_next_power_2(u64 s) {
  /*
   * Computes the next power of 2 greater than or equal to s.
   *
   * Strategy: Fill all bits to the right of the highest set bit with 1s,
   * then add 1 to get the next power of 2.
   *
   * Example with s = 0b00010110 (22):
   *   s = 22        -> 0b00010110
   *   s--           -> 0b00010101  (subtract 1 to handle exact powers of 2)
   *   s |= s >> 1   -> 0b00011111  (propagate highest bit 1 position right)
   *   s |= s >> 2   -> 0b00011111  (propagate 2 positions right)
   *   s |= s >> 4   -> 0b00011111  (propagate 4 positions right)
   *   s |= s >> 8   -> 0b00011111  (propagate 8 positions right)
   *   s |= s >> 16  -> 0b00011111  (propagate 16 positions right)
   *   s |= s >> 32  -> 0b00011111  (propagate 32 positions right, 64-bit)
   *   s + 1         -> 0b00100000  (32, the next power of 2)
   *
   * Why s--? If s is already a power of 2, we want to return s itself,
   * not the next one. Subtracting 1 ensures this case works correctly.
   *
   * The cascading OR operations create a mask of all 1s from the highest
   * set bit down to the LSB. Adding 1 then produces the next power of 2.
   */

  if (s == 0)
    return 1;
  s--;

  s |= s >> 1;
  s |= s >> 2;
  s |= s >> 4;
  s |= s >> 8;
  s |= s >> 16;

  if (S_WORD > 4)
    s |= s >> 32;

  return s + 1;
}

u8 __arena_utils_bit_count(const u8 byte) {
  u8 count = 0;

  /*
   * In bit count, there are 2 possible cases,
   * we only look at the most significant bit to the right:
   *
   * 1)
   * w := 1w'1
   * w &= w - 1 -> w = 1w'0
   *
   * 2)
   * w := 1w'0^k
   * w &= w - 1 -> w &= 1w''01^k -> w = 1w''0^(k + 1)
   *
   * Example:
   *  - 0b111, 0
   *  - 0b111 & 0b110 = 0b110, 1
   *  - 0b110 & 0b101 = 0b100, 2
   *  - 0b100 & 0b011 = 0b000, 3
   *
   *  - 0b101, 0
   *  - 0b101 & 0b100 = 0b100, 1
   *  - 0b100 & 0b011 = 0b000, 2
   *
   * So, this function clears the least significant bit
   * at each iteration, basically counting how many there are.
   * */

  for (u8 b = byte; b > 0; b &= (b - 1), count++);
  return count;
}

static inline u64 __alloc_utils_ceil(const f64 x) {
  return (u64)x + (x > (f64)((u64)x) ? 1 : 0);
}

static inline u64 __alloc_utils_max(const u64 a, const u64 b) {
  return a >= b ? a : b;
}

static inline u64 __alloc_utils_min(const u64 a, const u64 b) {
  return a <= b ? a : b;
}

static inline void __alloc_utils_swap(u64* a, u64* b) {
  *a ^= *b;
  *b ^= *a;
  *a ^= *b;
}
