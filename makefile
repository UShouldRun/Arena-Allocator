# Compiler and flags
CC = gcc
# Added -Wextra and -fPIC (common for libraries)
CFLAGS = -Wall -Wextra -Werror -Iinclude -g

# Directory structure
BUILD_DIR = build
SRC_DIR   = src
INC_DIR   = include

# Target names
TARGET_NAME = alloc
TARGET_LIB  = $(BUILD_DIR)/lib$(TARGET_NAME).a

# Find source files and define corresponding object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

COMMIT =?

# Phony targets
.PHONY: all build install commit push clean

# Default target
all: build

# Build target: create the static library
build: $(TARGET_LIB)

# Rule to create the static library from object files
$(TARGET_LIB): $(OBJS)
	ar rcs $@ $^

# Rule to compile object files
# Ensures headers in include/ are tracked
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Install target: copy the library and headers to system directories
# Standard practice uses 'sudo' or expects sufficient permissions
install: build
	install -d /usr/local/lib
	install -m 644 $(TARGET_LIB) /usr/local/lib/
	install -d /usr/local/include/$(TARGET_NAME)
	install -m 644 $(INC_DIR)/*.h /usr/local/include/$(TARGET_NAME)/

commit:
	git add .
	git commit -m "$(COMMIT)"

push: commit
	git push origin main

# Clean target: remove the build directory entirely
clean:
	rm -rf $(BUILD_DIR)
