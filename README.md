# Memory Allocator Library (Arena & Pool)

## Introduction
This is a custom memory allocator library implemented in C that provides two distinct allocation strategies: **Arena** (linear allocator) and **Pool** (best-fit block allocator). Each allocator is optimized for different use cases:

* **Arena**: Ideal for per-frame or per-task allocations where all memory is released at once. No individual frees supported.
* **Pool**: Designed for long-lived objects of varying sizes where individual allocation and deallocation is needed.

Both allocators offer performance benefits over traditional dynamic memory allocation for their specific use cases.

## Features
The library provides two allocator types as defined in `alloc.h`:

### Arena (Linear Allocator)
* **Characteristics:**
    * O(1) allocation and reset
    * No individual frees - reset or destroy entire arena
    * Uses contiguous buffer with pointer-increment logic
    * 8-byte size headers for each allocation

* **Functions:**
    * `arena_create`: Initialize arena with specified size and max nodes
    * `arena_alloc`: Allocate a block of memory
    * `arena_alloc_array`: Allocate memory for an array of objects
    * `arena_realloc`: Resize a previously allocated block
    * `arena_strdup`: Duplicate a string within the arena
    * `arena_reset`: Reset arena for reuse without deallocating
    * `arena_destroy`: Free all memory associated with the arena
    * `arena_print`: Print debug information to a file stream
    * `arena_get_size`: Get total arena size
    * `arena_get_size_used`: Get currently used memory
    * `arena_get_size_nodes`: Get current number of nodes
    * `arena_get_size_nodes_max`: Get maximum number of nodes

### Pool (Best-Fit Block Allocator)
* **Characteristics:**
    * O(F) allocation and free (F = free list length)
    * Individual frees supported with automatic coalescing
    * Best-fit strategy with sorted free list
    * Uses internal Arena for metadata management

* **Functions:**
    * `pool_create`: Initialize pool with size, block size, and max nodes
    * `pool_alloc`: Allocate a block of memory
    * `pool_alloc_array`: Allocate memory for an array of objects
    * `pool_realloc`: Resize a previously allocated block
    * `pool_strdup`: Duplicate a string within the pool
    * `pool_free`: Free an individual allocation
    * `pool_reset`: Reset pool for reuse
    * `pool_destroy`: Free all memory associated with the pool
    * `pool_print`: Print debug information to a file stream
    * `pool_get_size`: Get total pool size
    * `pool_get_size_used`: Get currently used memory
    * `pool_get_size_nodes`: Get current number of nodes
    * `pool_get_size_nodes_max`: Get maximum number of nodes

### Utility Macros
* `KB(x)`: Convert kilobytes to bytes
* `MB(x)`: Convert megabytes to bytes
* `GB(x)`: Convert gigabytes to bytes

## Building
To build the static library, use the provided `Makefile`. The `make build` command will compile the source code and create `liballoc.a` in the `build/` directory.

```bash
make build
```

## Installation
To install the library and its headers to standard system directories (`/usr/local/lib` and `/usr/local/include`), use the `make install` command.

```bash
make install
```

## Example Usage

### Arena Example
```c
#include "alloc.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
  // Create an arena with 1MB of memory and up to 1000 nodes
  Arena* arena = arena_create(MB(1), 1000);
  assert(arena != NULL);

  // Allocate some memory
  char* buffer = arena_alloc(arena, 256);
  assert(buffer != NULL);

  strcpy(buffer, "Hello, Arena!");
  printf("Allocated string: %s\n", buffer);

  // Allocate an array
  int* numbers = arena_alloc_array(arena, sizeof(int), 10);
  assert(numbers != NULL);

  for (int i = 0; i < 10; i++)
    numbers[i] = i * i;

  // Duplicate a string
  char* dup_str = arena_strdup(arena, "This is a duplicated string");
  assert(dup_str != NULL);
  printf("Duplicated string: %s\n", dup_str);

  // Print arena information
  printf("Arena size: %llu bytes\n", arena_get_size(arena));
  printf("Arena used: %llu bytes\n", arena_get_size_used(arena));

  // Debug print to stdout
  arena_print(arena, stdout);

  // Clean up - this frees all allocated memory at once
  arena_destroy(arena);

  return 0;
}
```

### Pool Example
```c
#include "alloc.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
  // Create a pool with 1MB total, 64-byte blocks, and up to 1000 nodes
  Pool* pool = pool_create(MB(1), 64, 1000);
  assert(pool != NULL);

  // Allocate some memory
  char* buffer1 = pool_alloc(pool, 128);
  assert(buffer1 != NULL);
  strcpy(buffer1, "Hello, Pool!");

  // Allocate another block
  int* numbers = pool_alloc_array(pool, sizeof(int), 20);
  assert(numbers != NULL);

  for (int i = 0; i < 20; i++)
    numbers[i] = i * i;

  // Free individual allocation - this is unique to Pool
  pool_free(pool, buffer1);

  // Allocate again - reuses freed memory
  char* buffer2 = pool_alloc(pool, 256);
  assert(buffer2 != NULL);
  strcpy(buffer2, "Reused memory!");

  // Print pool information
  printf("Pool size: %llu bytes\n", pool_get_size(pool));
  printf("Pool used: %llu bytes\n", pool_get_size_used(pool));

  // Debug print to stdout
  pool_print(pool, stdout);

  // Clean up
  pool_destroy(pool);

  return 0;
}
```

## License
This project is licensed under the MIT License. See the LICENSE file for details.

## Author
**Henrique Teixeira**
