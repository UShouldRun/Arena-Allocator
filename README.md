# Arena Memory Allocator

## Introduction
This is a custom memory allocator library implemented in C. It provides an "arena" or "pool" based memory management system, which is useful for scenarios where a large number of temporary allocations are performed within a limited scope. An arena allocator can offer performance benefits over traditional dynamic memory allocation for specific use cases.

## Features
The library provides a set of functions for managing memory within a dedicated arena, as defined in `include/arena.h`:

* **Creation and Destruction:**
    * `arena_create`: Initializes a new memory arena with a specified size and maximum number of nodes.
    * `arena_create_aligned`: Initializes an arena with a specific block alignment.
    * `arena_destroy`: Frees all memory associated with the arena.

* **Allocation and Deallocation:**
    * `arena_alloc`: Allocates a block of memory from the arena.
    * `arena_alloc_array`: Allocates a block for an array of objects.
    * `arena_realloc`: Resizes a previously allocated block.
    * `arena_free`: Frees a block of memory.

* **Utility Functions:**
    * `arena_strdup`: Allocates and duplicates a string within the arena.
    * `arena_is_aligned`: Checks if the arena is aligned.
    * `arena_reset`: Resets the arena for reuse without re-allocating memory.
    * `arena_print`: Prints debug information about the arena to a file stream.

* **Information Functions:**
    * `arena_get_size`: Get the total size of the arena's memory region.
    * `arena_get_size_used`: Get the amount of memory currently in use.
    * `arena_get_size_bitmap`: Get the size of the internal bitmap used for tracking.

## Building
To build the static library, use the provided `Makefile`. The `make build` command will compile the source code and create `libarena.a` in the `build/` directory.

```bash
make build
```

## Installation
To install the library and its headers to standard system directories (`/usr/local/lib` and `/usr/local/include`), use the `make install` command.

```bash
make install
```

## Example Usage

```c
#include "arena.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
  // Create an arena with 1MB of memory and up to 1000 nodes
  Arena arena = arena_create(1024 * 1024, 1000);
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
  assert(numbers != NULL);
  printf("Duplicated string: %s\n", dup_str);

  // Print arena information
  printf("Arena size: %zu bytes\n", arena_get_size(arena));
  printf("Arena used: %zu bytes\n", arena_get_size_used(arena));

  // Debug print to stdout
  arena_print(arena, stdout);

  // Clean up - this frees all allocated memory at once
  arena_destroy(arena);

  return 0;
}
```

## License
This project is licensed under the MIT License. See the LICENSE file for details.

## Author
**Henrique Teixeira**
