# Compiler and flags
CC = gcc
CFLAGS = -Wall -Werror -Iinclude

# Directory structure
BUILD_DIR = build
SRC_DIR = src
INC_DIR = include

# Target names
TARGET_NAME = arena
TARGET_LIB = $(BUILD_DIR)/lib$(TARGET_NAME).a

# Find source files and define corresponding object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Phony targets to ensure they are always run
.PHONY: all build install clean

# Default target
all: build

# Build target: create the static library
build: $(TARGET_LIB)

# Rule to compile object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to create the static library from object files
$(TARGET_LIB): $(OBJS)
	ar rcs $@ $^

# Install target: copy the library and headers to system directories
install: build
	cp $(TARGET_LIB) /usr/local/lib/
	cp $(INC_DIR)/*.h /usr/local/include/

# Clean target: remove all built files
clean:
	rm -f $(BUILD_DIR)/*.o $(TARGET_LIB)
